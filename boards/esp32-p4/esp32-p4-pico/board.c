/*
 * ESP32-P4-Pico storage: a microSD on 4-bit SDMMC, powered by the chip's on-chip
 * LDO. Extracted from main.c so the board's wiring lives with the board. Falls back
 * to 1-bit @ 400 kHz if the 4-bit mount doesn't take.
 */
#include "board.h"

/* The whole microSD implementation is compiled only when the card is supported
 * (-DPHP_STORAGE_MICROSD, the default). With it off the SD drivers aren't even linked;
 * main.c doesn't call board_mount_storage() in that case. */
#ifdef PHP_STORAGE_MICROSD

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

static const char *TAG = "board";

/* microSD wiring on the ESP32-P4-Pico (4-bit SDMMC), from the board schematic. */
#define SD_PIN_CLK  GPIO_NUM_43
#define SD_PIN_CMD  GPIO_NUM_44
#define SD_PIN_D0   GPIO_NUM_39
#define SD_PIN_D1   GPIO_NUM_40
#define SD_PIN_D2   GPIO_NUM_41
#define SD_PIN_D3   GPIO_NUM_42
/* The card is powered by the on-chip LDO, channel 4. */
#define SD_LDO_CHAN 4

static sdmmc_card_t *s_card;
static sd_pwr_ctrl_handle_t s_pwr_ctrl;

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
