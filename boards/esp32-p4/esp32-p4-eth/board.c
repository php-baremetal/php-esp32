/*
 * ESP32-P4-ETH storage: a microSD on 4-bit SDMMC, powered by the chip's on-chip LDO,
 * with the card's VDD switch enabled from a GPIO first. Same SD reference design as the
 * P4-Pico; the only board difference is the GPIO45 power switch. Falls back to 1-bit @
 * 400 kHz if the 4-bit mount doesn't take.
 */
#include "board.h"

#include "esp_log.h"
#include "soc/gpio_num.h"     /* GPIO_NUM_* constants (SD pins + the PHY reset line) */

/* microSD drivers -- only when the card is supported (-DPHP_STORAGE_MICROSD, the default). */
#ifdef PHP_STORAGE_MICROSD
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

#include "esp_netif.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "esp_eth_mac_esp.h"   /* ETH_ESP32_EMAC_DEFAULT_CONFIG(), esp_eth_mac_new_esp32() */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "board";

#ifdef PHP_STORAGE_MICROSD
/* microSD wiring on the ESP32-P4-ETH (4-bit SDMMC), traced from the schematic (U6/TF1).
 * Identical pin-out to the P4-Pico. */
#define SD_PIN_CLK  GPIO_NUM_43
#define SD_PIN_CMD  GPIO_NUM_44
#define SD_PIN_D0   GPIO_NUM_39
#define SD_PIN_D1   GPIO_NUM_40
#define SD_PIN_D2   GPIO_NUM_41
#define SD_PIN_D3   GPIO_NUM_42
/* The SDMMC IO voltage domain (VDDPST_5) is fed by the chip's on-chip LDO, channel 4. */
#define SD_LDO_CHAN 4
/* The card's VDD is switched by a high-side P-MOSFET (Q1) from 3V3: GPIO45 low turns it
 * on. A 10K pulldown already defaults it on, but we drive it so power is deterministic. */
#define SD_PIN_PWR_EN GPIO_NUM_45

static sdmmc_card_t *s_card;
static sd_pwr_ctrl_handle_t s_pwr_ctrl;

/* Enable the card's VDD (GPIO45 low = powered). Non-fatal: the 10K pulldown means the
 * card is already powered by default, so a failure here just leaves that in place. */
static void sd_power_on(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << SD_PIN_PWR_EN,
        .mode = GPIO_MODE_OUTPUT,
    };
    if (gpio_config(&io) == ESP_OK) {
        gpio_set_level(SD_PIN_PWR_EN, 0);
    }
}

static esp_err_t mount_try(const char *mount_point, int width, int freq_khz)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.pwr_ctrl_handle = s_pwr_ctrl;   /* on-chip LDO that powers the SD IO domain */
    if (freq_khz) {
        host.max_freq_khz = freq_khz;
    }

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = width;
    slot.clk = SD_PIN_CLK;
    slot.cmd = SD_PIN_CMD;
    slot.d0  = SD_PIN_D0;
    slot.d1  = SD_PIN_D1;
    slot.d2  = SD_PIN_D2;
    slot.d3  = SD_PIN_D3;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    return esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot, &mount_config, &s_card);
}

bool board_mount_storage(const char *mount_point)
{
    sd_power_on();

    sd_pwr_ctrl_ldo_config_t ldo_config = { .ldo_chan_id = SD_LDO_CHAN };
    if (sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &s_pwr_ctrl) != ESP_OK) {
        ESP_LOGE(TAG, "could not enable SD LDO (channel %d)", SD_LDO_CHAN);
        return false;
    }
    if (mount_try(mount_point, 4, 0) == ESP_OK) {
        return true;
    }
    ESP_LOGW(TAG, "4-bit mount failed; retrying 1-bit @ 400 kHz");
    if (mount_try(mount_point, 1, SDMMC_FREQ_PROBING) == ESP_OK) {
        return true;
    }
    return false;
}

void board_unmount_storage(const char *mount_point)
{
    if (s_card) {
        esp_vfs_fat_sdcard_unmount(mount_point, s_card);
        s_card = NULL;
    }
}
#endif /* PHP_STORAGE_MICROSD */

/* Ethernet PHY reset line (IP101 pin 32), driven by the esp_eth driver. RMII pin map (from the
 * schematic, matching the ESP32-P4 EMAC default): REF_CLK=GPIO50, TX_EN=49, TXD0/1=34/35,
 * RXD0/1=29/30, CRS_DV=28, MDC=31, MDIO=52. */
#define PHY_RESET_GPIO GPIO_NUM_51
/* How long to wait for a DHCP lease before giving up (link-up + negotiation). */
#define NET_DHCP_TIMEOUT_MS 15000

bool board_network_up(char *ip_out, size_t ip_len)
{
    /* esp_netif + the default event loop drive the netif and its DHCP client. Both are
     * idempotent-ish; treat "already created" as fine so a second call doesn't fail. */
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init: %s", esp_err_to_name(err));
        return false;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop: %s", esp_err_to_name(err));
        return false;
    }

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *netif = esp_netif_new(&netif_cfg);

    /* EMAC: the ESP32-P4 default pin map is exactly this board's RMII wiring
     * (MDC=31, MDIO=52, REF_CLK in on 50, TX_EN=49, TXD0/1=34/35, CRS_DV=28, RXD0/1=29/30). */
    eth_esp32_emac_config_t esp32_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_cfg, &mac_cfg);

    /* PHY: IP101, hardware-reset on GPIO51, address auto-detected off the straps. */
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr = ESP_ETH_PHY_ADDR_AUTO;
    phy_cfg.reset_gpio_num = PHY_RESET_GPIO;
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_cfg);

    if (!mac || !phy) {
        ESP_LOGE(TAG, "could not create Ethernet MAC/PHY");
        return false;
    }

    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth = NULL;
    if ((err = esp_eth_driver_install(&eth_cfg, &eth)) != ESP_OK) {
        ESP_LOGE(TAG, "eth driver install: %s", esp_err_to_name(err));
        return false;
    }
    if ((err = esp_netif_attach(netif, esp_eth_new_netif_glue(eth))) != ESP_OK) {
        ESP_LOGE(TAG, "netif attach: %s", esp_err_to_name(err));
        return false;
    }
    if ((err = esp_eth_start(eth)) != ESP_OK) {
        ESP_LOGE(TAG, "eth start: %s", esp_err_to_name(err));
        return false;
    }

    /* Poll for the DHCP lease: the default ETH netif runs the DHCP client once the link
     * is up, and the default event loop task drives it in the background. */
    const int steps = NET_DHCP_TIMEOUT_MS / 100;
    for (int i = 0; i < steps; i++) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0) {
            esp_ip4addr_ntoa(&ip.ip, ip_out, ip_len);
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return false;
}
