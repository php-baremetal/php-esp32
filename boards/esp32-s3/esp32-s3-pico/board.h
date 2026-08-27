/*
 * ESP32-S3-Pico board support (a plain ESP32-S3 with a microSD slot, no wired network).
 *
 * Like every board, this one owns everything specific to its wiring: the microSD pins and
 * the mount code in board.c. main.c is board-agnostic and talks only to this interface.
 *
 * The ESP32-S3 has no internal SD host, so the card hangs off SPI here:
 *   - microSD in SPI mode (SD_MOSI=6, SD_MISO=5, SD_CLK=7, SD_CS=4).
 *
 * There is no wired Ethernet on this board -- it is the S3-ETH without the W5500 -- so it
 * does NOT define BOARD_HAS_NETWORK and provides no board_network_up(). The network instead
 * comes from the S3's built-in Wi-Fi, driven from PHP by the `wifi` extension; the `web-server`
 * model works over that (the httpd binds regardless of any wired link).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define BOARD_NAME "ESP32-S3-Pico"  /* the specific board (identity for tooling) */
#define BOARD_SOC  "ESP32-S3"       /* the SoC/family, printed in the console banner */

/* This board has a microSD slot: it provides board_mount_storage(). Boards without a slot
 * (the `-zero` variants) leave this undefined, so the SD path is never built or probed. */
#define BOARD_HAS_MICROSD 1

/* No BOARD_HAS_NETWORK: this board has no *wired* interface, so main.c skips the
 * bring-up-network-at-boot path entirely and no board_network_up() is declared. Wi-Fi is
 * brought up from PHP (the `wifi` extension), not by the board layer. */

/* Mount the board's storage at mount_point. On this board that's a microSD in SPI mode.
 * Returns true on success. */
bool board_mount_storage(const char *mount_point);

/* Unmount what board_mount_storage() mounted (no-op if nothing is mounted). */
void board_unmount_storage(const char *mount_point);
