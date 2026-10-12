/*
 * ESP32-S3 chip board interface -- one header for every S3 board.
 *
 * main.c is board-agnostic and talks only to this interface. Which peripherals exist is
 * decided by the project/preset config, not the board identity: the SD-SPI and W5500 code
 * in board.c is compiled only when the matching flag is set, and this header derives the
 * BOARD_HAS_* markers main.c checks from those same flags.
 *   - PHP_STORAGE_MICROSD -> BOARD_HAS_MICROSD, board_mount_storage()
 *   - PHP_NET_W5500       -> BOARD_HAS_NETWORK, board_network_up()
 * The S3 always has a Wi-Fi radio on-die, reachable from PHP via the `wifi` extension, so a
 * bare board (neither flag) still serves over Wi-Fi without any wired bring-up here.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

/* The specific board's display name: a preset passes -DPHP_BOARD_NAME; otherwise this chip
 * default stands. BOARD_SOC is the SoC, printed in the console banner. */
#ifndef BOARD_NAME
#define BOARD_NAME "ESP32-S3"
#endif
#define BOARD_SOC "ESP32-S3"

#ifdef PHP_STORAGE_MICROSD
#define BOARD_HAS_MICROSD 1

/* Mount the microSD (SD-over-SPI) at mount_point. Returns true on success. */
bool board_mount_storage(const char *mount_point);

/* Unmount what board_mount_storage() mounted (no-op if nothing is mounted). */
void board_unmount_storage(const char *mount_point);
#endif

#ifdef PHP_NET_W5500
#define BOARD_HAS_NETWORK 1

/* Bring up the W5500 wired Ethernet and wait (a bounded time) for a DHCP lease. On success
 * writes the dotted-decimal IPv4 address into ip_out (needs >= 16 bytes) and returns true;
 * returns false if the link stays down or no lease arrives in time. */
bool board_network_up(char *ip_out, size_t ip_len);
#endif
