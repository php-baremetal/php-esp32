/*
 * ESP32-S3-ETH board support.
 *
 * Like every board, this one owns everything specific to its wiring: the microSD pins
 * and the Ethernet wiring, plus the mount/network code in board.c. main.c is
 * board-agnostic and talks only to this interface.
 *
 * The ESP32-S3 has neither an internal SD host nor an internal Ethernet MAC, so both
 * peripherals hang off SPI here:
 *   - microSD in SPI mode (SD_MOSI=6, SD_MISO=5, SD_CLK=7, SD_CS=4);
 *   - a W5500 SPI Ethernet controller (MAC+PHY in one chip: MOSI=11, MISO=12, SCLK=13,
 *     CS=14, INT=10, RST=9) -- which is why this board's board.toml advertises the
 *     `web-server` project type. Each sits on its own SPI host, so they don't share pins.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define BOARD_NAME "ESP32-S3-ETH"   /* the specific board (identity for tooling) */
#define BOARD_SOC  "ESP32-S3"       /* the SoC/family, printed in the console banner */

/* This board has wired Ethernet (a W5500 SPI controller). main.c brings the network up at
 * boot for any board that defines BOARD_HAS_NETWORK, and logs the address. */
#define BOARD_HAS_NETWORK 1

/* This board has a microSD slot: it provides board_mount_storage(). Boards without a slot
 * (the `-zero` variants) leave this undefined, so the SD path is never built or probed. */
#define BOARD_HAS_MICROSD 1

/* Bring up Ethernet and wait (a bounded time) for a DHCP lease. On success writes the
 * dotted-decimal IPv4 address into ip_out (needs >= 16 bytes) and returns true; returns
 * false if the link stays down or no lease arrives in time. */
bool board_network_up(char *ip_out, size_t ip_len);

/* Mount the board's storage at mount_point. On this board that's a microSD in SPI mode.
 * Returns true on success. */
bool board_mount_storage(const char *mount_point);

/* Unmount what board_mount_storage() mounted (no-op if nothing is mounted). */
void board_unmount_storage(const char *mount_point);
