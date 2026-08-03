/*
 * ESP32-P4-ETH board support (Waveshare).
 *
 * Like every board, this one owns everything specific to its wiring: the microSD
 * pins and power, and the mount code in board.c. main.c is board-agnostic and talks
 * only to this interface.
 *
 * Storage is the same 4-bit SDMMC microSD as the P4-Pico (Waveshare reuse the P4 SD
 * reference design), with one addition: the card's VDD is gated by a P-MOSFET driven
 * from GPIO45, which board.c enables before mounting.
 *
 * This board also carries a wired Ethernet PHY (IP101GRI, RMII) -- which is why its
 * board.toml advertises the `web-server` project type. Networking isn't implemented in
 * the firmware yet, so there is no Ethernet code here; the RMII pin map is recorded in
 * board.c for when it is.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define BOARD_NAME "ESP32-P4-ETH"   /* the specific board (identity for tooling) */
#define BOARD_SOC  "ESP32-P4"       /* the SoC/family, printed in the console banner */

/* This board has wired Ethernet (IP101 RMII PHY). main.c brings the network up at boot
 * for any board that defines BOARD_HAS_NETWORK, and logs the address. */
#define BOARD_HAS_NETWORK 1

/* Bring up Ethernet and wait (a bounded time) for a DHCP lease. On success writes the
 * dotted-decimal IPv4 address into ip_out (needs >= 16 bytes) and returns true; returns
 * false if the link stays down or no lease arrives in time. */
bool board_network_up(char *ip_out, size_t ip_len);

/* Mount the board's storage at mount_point. On this board that's a 4-bit SDMMC
 * microSD powered by the on-chip LDO (channel 4), with its VDD switch (GPIO45)
 * enabled first. Returns true on success. */
bool board_mount_storage(const char *mount_point);

/* Unmount what board_mount_storage() mounted (no-op if nothing is mounted). */
void board_unmount_storage(const char *mount_point);
