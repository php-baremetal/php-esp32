/*
 * ESP32-S3-Pico storage (a plain ESP32-S3 with a microSD slot, no wired network).
 *
 * The ESP32-S3 has no internal SD host, so the microSD is an SD card in SPI mode on SPI2
 * (esp_vfs_fat_sdspi_mount). There is no Ethernet hardware on this board, so it provides
 * no board_network_up() -- main.c never calls one because BOARD_HAS_NETWORK is not defined.
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

static const char *TAG = "board";

/* microSD wiring on the ESP32-S3-Pico -- SD card in SPI mode (from the schematic). */
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
