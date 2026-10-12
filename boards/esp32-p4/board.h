/*
 * ESP32-P4 chip board interface -- one header for every P4 board.
 *
 * main.c is board-agnostic and talks only to this interface. Which peripherals exist is
 * decided by the project/preset config, not the board identity: the SDMMC and EMAC code
 * in board.c is compiled only when the matching flag is set, and this header derives the
 * BOARD_HAS_* markers main.c checks from those same flags.
 *   - PHP_STORAGE_MICROSD -> BOARD_HAS_MICROSD, board_mount_storage()
 *   - PHP_NET_EMAC        -> BOARD_HAS_NETWORK, board_network_up()
 * The P4 has no on-die radio: WiFi runs on an ESP32-C6 companion over ESP-HOSTED, reachable
 * from PHP via the `wifi` extension (the wifi-c6 preset supplies the ESP-HOSTED sdkconfig),
 * with no wired bring-up here.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

/* The specific board's display name: a preset passes -DPHP_BOARD_NAME; otherwise this chip
 * default stands. BOARD_SOC is the SoC, printed in the console banner. */
#ifndef BOARD_NAME
#define BOARD_NAME "ESP32-P4"
#endif
#define BOARD_SOC "ESP32-P4"

#ifdef PHP_STORAGE_MICROSD
#define BOARD_HAS_MICROSD 1

/* Mount the microSD (4-bit SDMMC) at mount_point. Returns true on success. */
bool board_mount_storage(const char *mount_point);

/* Unmount what board_mount_storage() mounted (no-op if nothing is mounted). */
void board_unmount_storage(const char *mount_point);
#endif

#ifdef PHP_NET_EMAC
#define BOARD_HAS_NETWORK 1

/* Bring up the internal-EMAC wired Ethernet (IP101 RMII PHY) and wait (a bounded time) for a
 * DHCP lease. On success writes the dotted-decimal IPv4 address into ip_out (needs >= 16 bytes)
 * and returns true; returns false if the link stays down or no lease arrives in time. */
bool board_network_up(char *ip_out, size_t ip_len);
#endif
