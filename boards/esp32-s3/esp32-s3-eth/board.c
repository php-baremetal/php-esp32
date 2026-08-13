/*
 * ESP32-S3-ETH storage + networking (Waveshare).
 *
 * The ESP32-S3 has no internal SD host and no internal Ethernet MAC, so both peripherals
 * are SPI devices on this board, each on its own SPI host:
 *   - the microSD is an SD card in SPI mode on SPI2 (esp_vfs_fat_sdspi_mount);
 *   - the wired Ethernet is a W5500 (MAC+PHY over SPI) on SPI3, driven by the esp_eth
 *     W5500 driver with lwIP on top -- exactly the same board_network_up() contract the
 *     P4-ETH's internal-EMAC path presents to main.c.
 */
#include "board.h"

#include "esp_log.h"
#include "soc/gpio_num.h"

/* microSD drivers -- only when the card is supported (-DPHP_STORAGE_MICROSD, the default). */
#ifdef PHP_STORAGE_MICROSD
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#endif

#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "esp_eth_mac_spi.h"   /* ETH_W5500_DEFAULT_CONFIG, esp_eth_mac_new_w5500 */
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "board";

#ifdef PHP_STORAGE_MICROSD
/* microSD wiring on the ESP32-S3-ETH -- SD card in SPI mode (from the schematic). */
#define SD_SPI_HOST  SPI2_HOST
#define SD_PIN_MOSI  GPIO_NUM_6
#define SD_PIN_MISO  GPIO_NUM_5
#define SD_PIN_CLK   GPIO_NUM_7
#define SD_PIN_CS    GPIO_NUM_4

static sdmmc_card_t *s_card;
static bool s_sd_bus_ready;

static esp_err_t mount_try(const char *mount_point, int freq_khz)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;
    if (freq_khz) {
        host.max_freq_khz = freq_khz;
    }

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = SD_PIN_CS;
    slot.host_id = SD_SPI_HOST;

    return esp_vfs_fat_sdspi_mount(mount_point, &host, &slot, &mount_config, &s_card);
}

bool board_mount_storage(const char *mount_point)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_MOSI,
        .miso_io_num = SD_PIN_MISO,
        .sclk_io_num = SD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    esp_err_t err = spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SD SPI bus init: %s", esp_err_to_name(err));
        return false;
    }
    s_sd_bus_ready = true;

    if (mount_try(mount_point, 0) == ESP_OK) {
        return true;
    }
    ESP_LOGW(TAG, "SD mount failed at default speed; retrying @ 400 kHz");
    if (mount_try(mount_point, SDMMC_FREQ_PROBING) == ESP_OK) {
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
    if (s_sd_bus_ready) {
        spi_bus_free(SD_SPI_HOST);
        s_sd_bus_ready = false;
    }
}
#endif /* PHP_STORAGE_MICROSD */

/* W5500 Ethernet wiring on the ESP32-S3-ETH (from the schematic). The W5500 is a full
 * MAC+PHY on SPI; the esp_eth W5500 driver presents it as a standard netif so lwIP runs
 * on top -- so the DHCP/poll logic below is identical to the P4-ETH's. */
#define ETH_SPI_HOST      SPI3_HOST
#define ETH_PIN_MOSI      GPIO_NUM_11
#define ETH_PIN_MISO      GPIO_NUM_12
#define ETH_PIN_SCLK      GPIO_NUM_13
#define ETH_PIN_CS        GPIO_NUM_14
#define ETH_PIN_INT       GPIO_NUM_10
#define ETH_PIN_RST       GPIO_NUM_9
/* W5500 SPI clock. 20 MHz is the reliable figure Waveshare/Espressif use for this module
 * (the part can do more, but margins on the SPI traces make 20 MHz the safe default). */
#define ETH_SPI_CLOCK_MHZ 20
/* How long to wait for a DHCP lease before giving up (link-up + negotiation). */
#define NET_DHCP_TIMEOUT_MS 15000
/* How long to wait for the PHY link before deciding there is no cable. Ethernet is a
 * board feature, not a requirement: a sketch that doesn't touch the network should not
 * pay the full DHCP timeout at boot just because the port is unplugged. */
#define NET_LINK_TIMEOUT_MS 2000

/* Set from the Ethernet driver's events so board_network_up() can tell "no cable" (the
 * link never comes up) from "link up, DHCP still negotiating". */
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
    /* Track link up/down (harmless if already registered on a second call). */
    esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_link_event, NULL);

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *netif = esp_netif_new(&netif_cfg);

    /* The W5500 hangs off its own SPI host (SPI3), separate from the microSD's (SPI2). */
    spi_bus_config_t buscfg = {
        .mosi_io_num = ETH_PIN_MOSI,
        .miso_io_num = ETH_PIN_MISO,
        .sclk_io_num = ETH_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    err = spi_bus_initialize(ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "ETH SPI bus init: %s", esp_err_to_name(err));
        return false;
    }

    /* The W5500 driver uses a GPIO interrupt for RX; it needs the shared ISR service. */
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio ISR service: %s", esp_err_to_name(err));
        return false;
    }

    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20,
        .spics_io_num = ETH_PIN_CS,
    };

    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_w5500_config_t w5500_cfg = ETH_W5500_DEFAULT_CONFIG(ETH_SPI_HOST, &spi_devcfg);
    w5500_cfg.int_gpio_num = ETH_PIN_INT;
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_cfg, &mac_cfg);

    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr = 1;                 /* the W5500's internal PHY answers at address 1 */
    phy_cfg.reset_gpio_num = ETH_PIN_RST;
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_cfg);

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

    /* The W5500 has no factory-burned MAC address; derive one from the S3's own base MAC
     * (the Ethernet variant) so the interface has a stable, locally-unique address. */
    uint8_t mac_addr[6];
    if (esp_read_mac(mac_addr, ESP_MAC_ETH) == ESP_OK) {
        esp_eth_ioctl(eth, ETH_CMD_S_MAC_ADDR, mac_addr);
    }

    if ((err = esp_netif_attach(netif, esp_eth_new_netif_glue(eth))) != ESP_OK) {
        ESP_LOGE(TAG, "netif attach: %s", esp_err_to_name(err));
        return false;
    }
    if ((err = esp_eth_start(eth)) != ESP_OK) {
        ESP_LOGE(TAG, "eth start: %s", esp_err_to_name(err));
        return false;
    }

    /* Wait briefly for the PHY link. With no cable it never comes up, so return quickly
     * instead of blocking the whole DHCP timeout -- the sketch then runs right away. */
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

    /* Poll for the DHCP lease: the default ETH netif runs the DHCP client once the link is
     * up, and the default event loop task drives it in the background. */
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
