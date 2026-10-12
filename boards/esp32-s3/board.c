/*
 * ESP32-S3 chip board support -- one config-driven file for every S3 board.
 *
 * The S3 has no internal SD host nor Ethernet MAC, so both are SPI devices here. Each peripheral is
 * compiled in only when the project/preset asks for it, and its pins come from -D defines (with the
 * common module wiring as the fallback), so a preset or an off-catalog board just declares its pins:
 *   - microSD (SD-over-SPI): built when PHP_STORAGE_MICROSD is set; pins PHP_SD_{CS,CLK,MOSI,MISO}.
 *   - wired Ethernet (W5500): built when PHP_NET_W5500 is set; pins PHP_ETH_{CS,INT,RST,SCLK,MOSI,MISO}.
 * A bare board (no peripherals configured) compiles neither and runs embedded-only from flash.
 */
#include "board.h"

#include "esp_log.h"
#include "soc/gpio_num.h"

#if defined(PHP_STORAGE_MICROSD) || defined(PHP_NET_W5500)
static const char *TAG = "board";
#endif

/* ---- microSD (SD card in SPI mode) ------------------------------------------------------------ */
#ifdef PHP_STORAGE_MICROSD
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"

#ifndef PHP_SD_HOST
#define PHP_SD_HOST  SPI2_HOST
#endif
#ifndef PHP_SD_MOSI
#define PHP_SD_MOSI  GPIO_NUM_6
#endif
#ifndef PHP_SD_MISO
#define PHP_SD_MISO  GPIO_NUM_5
#endif
#ifndef PHP_SD_CLK
#define PHP_SD_CLK   GPIO_NUM_7
#endif
#ifndef PHP_SD_CS
#define PHP_SD_CS    GPIO_NUM_4
#endif

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
    host.slot = PHP_SD_HOST;
    if (freq_khz) {
        host.max_freq_khz = freq_khz;
    }

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = PHP_SD_CS;
    slot.host_id = PHP_SD_HOST;

    return esp_vfs_fat_sdspi_mount(mount_point, &host, &slot, &mount_config, &s_card);
}

bool board_mount_storage(const char *mount_point)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PHP_SD_MOSI,
        .miso_io_num = PHP_SD_MISO,
        .sclk_io_num = PHP_SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    esp_err_t err = spi_bus_initialize(PHP_SD_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
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
        spi_bus_free(PHP_SD_HOST);
        s_sd_bus_ready = false;
    }
}
#endif /* PHP_STORAGE_MICROSD */

/* ---- wired Ethernet (W5500: MAC+PHY over SPI) ------------------------------------------------- */
#ifdef PHP_NET_W5500
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "esp_eth_mac_spi.h"   /* ETH_W5500_DEFAULT_CONFIG, esp_eth_mac_new_w5500 */
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef PHP_ETH_HOST
#define PHP_ETH_HOST  SPI3_HOST
#endif
#ifndef PHP_ETH_MOSI
#define PHP_ETH_MOSI  GPIO_NUM_11
#endif
#ifndef PHP_ETH_MISO
#define PHP_ETH_MISO  GPIO_NUM_12
#endif
#ifndef PHP_ETH_SCLK
#define PHP_ETH_SCLK  GPIO_NUM_13
#endif
#ifndef PHP_ETH_CS
#define PHP_ETH_CS    GPIO_NUM_14
#endif
#ifndef PHP_ETH_INT
#define PHP_ETH_INT   GPIO_NUM_10
#endif
#ifndef PHP_ETH_RST
#define PHP_ETH_RST   GPIO_NUM_9
#endif
#ifndef PHP_ETH_SPI_CLOCK_MHZ
#define PHP_ETH_SPI_CLOCK_MHZ 20
#endif
#define NET_DHCP_TIMEOUT_MS 15000
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

    spi_bus_config_t buscfg = {
        .mosi_io_num = PHP_ETH_MOSI,
        .miso_io_num = PHP_ETH_MISO,
        .sclk_io_num = PHP_ETH_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    err = spi_bus_initialize(PHP_ETH_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "ETH SPI bus init: %s", esp_err_to_name(err));
        return false;
    }

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio ISR service: %s", esp_err_to_name(err));
        return false;
    }

    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = PHP_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20,
        .spics_io_num = PHP_ETH_CS,
    };

    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_w5500_config_t w5500_cfg = ETH_W5500_DEFAULT_CONFIG(PHP_ETH_HOST, &spi_devcfg);
    w5500_cfg.int_gpio_num = PHP_ETH_INT;
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_cfg, &mac_cfg);

    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr = 1;                 /* the W5500's internal PHY answers at address 1 */
    phy_cfg.reset_gpio_num = PHP_ETH_RST;
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
#endif /* PHP_NET_W5500 */
