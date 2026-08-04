/*
 * Discovery firmware -- a tiny throwaway app `phpflash discover --all` flashes to actively probe a
 * board's peripherals, which the ROM/esptool can't see. It prints a machine-readable block between
 * DISCOVER-FW-BEGIN/END that phpflash parses, then idles.
 *
 * It is built PER-BOARD and reuses that board's component (board.c) -- so the probes use the board's
 * real GPIO wiring, not a hardcoded pin map. phpflash builds it once per candidate board and sees
 * whose peripherals come up. "ethernet=yes" means that board's board_network_up() brought the link
 * up (needs the cable plugged in); "microsd=card:..." means its board_mount_storage() mounted a card.
 *
 * This OVERWRITES whatever app was on the board; phpflash warns and asks first, and reminds you to
 * re-flash your firmware afterwards.
 */
#include <stdio.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_mac.h"
#if CONFIG_SPIRAM
#include "esp_psram.h"
#endif
#include "esp_vfs_fat.h"   /* esp_vfs_fat_info() for the card size */

#include "board.h"         /* BOARD_NAME, BOARD_HAS_NETWORK, board_network_up(), board_mount_storage() */

static const char *chip_model(esp_chip_model_t m)
{
    switch (m) {
        case CHIP_ESP32:   return "ESP32";
        case CHIP_ESP32S2: return "ESP32-S2";
        case CHIP_ESP32S3: return "ESP32-S3";
        case CHIP_ESP32C3: return "ESP32-C3";
        case CHIP_ESP32C6: return "ESP32-C6";
        case CHIP_ESP32H2: return "ESP32-H2";
        case CHIP_ESP32P4: return "ESP32-P4";
        default:           return "unknown";
    }
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(300));   /* let the console settle so the block isn't split */

    printf("DISCOVER-FW-BEGIN\n");
    printf("board=%s\n", BOARD_NAME);   /* the board this build's wiring belongs to */

    esp_chip_info_t ci;
    esp_chip_info(&ci);
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BASE);

    printf("chip=%s\n", chip_model(ci.model));
    printf("cores=%d\n", ci.cores);
    printf("revision=%d\n", ci.revision);
#if CONFIG_SPIRAM
    printf("psram=%uMB\n", (unsigned) (esp_psram_get_size() / (1024 * 1024)));
#else
    printf("psram=none\n");
#endif
    printf("mac=%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

#ifdef BOARD_HAS_NETWORK
    char ip[16] = {0};
    bool net = board_network_up(ip, sizeof ip);
    printf("ethernet=%s\n", net ? "yes" : "no");   /* "no" here can also mean "no cable/DHCP" */
    if (net) {
        printf("ip=%s\n", ip);
    }
#else
    printf("ethernet=n/a\n");   /* this board defines no network hardware */
#endif

    if (board_mount_storage("/sd")) {
        uint64_t total = 0, freeb = 0;
        if (esp_vfs_fat_info("/sd", &total, &freeb) == ESP_OK) {
            printf("microsd=card:%" PRIu64 "MB\n", total / (1024ULL * 1024ULL));
        } else {
            printf("microsd=yes\n");
        }
    } else {
        printf("microsd=nocard\n");   /* empty slot vs no slot is indistinguishable */
    }

    printf("DISCOVER-FW-END\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
