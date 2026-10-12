/*
 * ESP32-P4 board support (one file for every P4 board). The peripheral blocks are compiled only
 * when the project/preset asks for them:
 *   - microSD (4-bit SDMMC/SDIO): built when PHP_STORAGE_MICROSD is set. The P4 SD reference wiring
 *     (CLK=43, CMD=44, D0..D3=39..42, on-chip LDO channel 4) is shared by every P4 board, so it is
 *     the built-in default; a board that switches the card's VDD from a GPIO sets PHP_SD_PWR_EN.
 *   - wired Ethernet (internal EMAC + IP101 RMII PHY): built when PHP_NET_EMAC is set; the PHY reset
 *     line is PHP_ETH_RST (default GPIO51), the RMII data/clock pins are the ESP32-P4 EMAC defaults.
 * WiFi on the P4 is not a board.c concern: it runs on an on-board ESP32-C6 companion over ESP-HOSTED,
 * brought up by the `wifi` extension (the wifi-c6 preset only supplies the ESP-HOSTED sdkconfig).
 */
#include "board.h"

#if defined(PHP_STORAGE_MICROSD) || defined(PHP_NET_EMAC)
#include "esp_log.h"
static const char *TAG = "board";
#endif

/* ---------------- microSD (4-bit SDMMC) ---------------- */
#ifdef PHP_STORAGE_MICROSD

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#ifdef PHP_SD_PWR_EN
#include "driver/gpio.h"
#endif

/* P4 SD reference wiring (4-bit SDMMC), shared by every P4 board. */
#ifndef PHP_SD_CLK
#define PHP_SD_CLK  GPIO_NUM_43
#endif
#ifndef PHP_SD_CMD
#define PHP_SD_CMD  GPIO_NUM_44
#endif
#ifndef PHP_SD_D0
#define PHP_SD_D0   GPIO_NUM_39
#endif
#ifndef PHP_SD_D1
#define PHP_SD_D1   GPIO_NUM_40
#endif
#ifndef PHP_SD_D2
#define PHP_SD_D2   GPIO_NUM_41
#endif
#ifndef PHP_SD_D3
#define PHP_SD_D3   GPIO_NUM_42
#endif
/* The card's IO domain is fed by the chip's on-chip LDO, channel 4. */
#ifndef PHP_SD_LDO_CHAN
#define PHP_SD_LDO_CHAN 4
#endif

static sdmmc_card_t *s_card;
static sd_pwr_ctrl_handle_t s_pwr_ctrl;

#ifdef PHP_SD_PWR_EN
/* Some boards gate the card's VDD with a high-side P-MOSFET: driving PHP_SD_PWR_EN low powers it.
 * Non-fatal -- a pulldown usually defaults it on -- so a failure just leaves that in place. */
static void sd_power_on(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PHP_SD_PWR_EN,
        .mode = GPIO_MODE_OUTPUT,
    };
    if (gpio_config(&io) == ESP_OK) {
        gpio_set_level(PHP_SD_PWR_EN, 0);
    }
}
#endif

static esp_err_t mount_try(const char *mount_point, int width, int freq_khz)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.pwr_ctrl_handle = s_pwr_ctrl;   /* on-chip LDO that powers the card */
    if (freq_khz) {
        host.max_freq_khz = freq_khz;
    }

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = width;
    slot.clk = PHP_SD_CLK;
    slot.cmd = PHP_SD_CMD;
    slot.d0  = PHP_SD_D0;
    slot.d1  = PHP_SD_D1;
    slot.d2  = PHP_SD_D2;
    slot.d3  = PHP_SD_D3;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    return esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot, &mount_config, &s_card);
}

bool board_mount_storage(const char *mount_point)
{
#ifdef PHP_SD_PWR_EN
    sd_power_on();
#endif
    sd_pwr_ctrl_ldo_config_t ldo_config = { .ldo_chan_id = PHP_SD_LDO_CHAN };
    if (sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &s_pwr_ctrl) != ESP_OK) {
        ESP_LOGE(TAG, "could not enable SD LDO (channel %d)", PHP_SD_LDO_CHAN);
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

/* ---------------- wired Ethernet (EMAC + IP101 RMII) ---------------- */
#ifdef PHP_NET_EMAC

#include "esp_netif.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "esp_eth_mac_esp.h"   /* ETH_ESP32_EMAC_DEFAULT_CONFIG(), esp_eth_mac_new_esp32() */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_num.h"

/* PHY reset line (IP101). The RMII data/clock pins use the ESP32-P4 EMAC default map
 * (REF_CLK=50, TX_EN=49, TXD0/1=34/35, RXD0/1=29/30, CRS_DV=28, MDC=31, MDIO=52). */
#ifndef PHP_ETH_RST
#define PHP_ETH_RST GPIO_NUM_51
#endif
#define NET_DHCP_TIMEOUT_MS 15000
/* Bound the wait for the PHY link so an unplugged port doesn't cost the full DHCP timeout at boot. */
#define NET_LINK_TIMEOUT_MS 2000

static volatile bool s_eth_link_up = false;

static void eth_link_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void) arg;
    (void) data;
    if (base != ETH_EVENT) {
        return;
    }
    if (id == ETHERNET_EVENT_CONNECTED) {
        s_eth_link_up = true;
    } else if (id == ETHERNET_EVENT_DISCONNECTED) {
        s_eth_link_up = false;
    }
}

bool board_network_up(char *ip_out, size_t ip_len)
{
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
    esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_link_event, NULL);

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *netif = esp_netif_new(&netif_cfg);

    eth_esp32_emac_config_t esp32_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_cfg, &mac_cfg);

    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr = ESP_ETH_PHY_ADDR_AUTO;
    phy_cfg.reset_gpio_num = PHP_ETH_RST;
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

    const int link_steps = NET_LINK_TIMEOUT_MS / 100;
    bool linked = false;
    for (int i = 0; i < link_steps; i++) {
        if (s_eth_link_up) {
            linked = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (!linked) {
        ESP_LOGW(TAG, "ethernet: no link (cable unplugged?) -- skipping DHCP wait");
        return false;
    }

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

#endif /* PHP_NET_EMAC */
