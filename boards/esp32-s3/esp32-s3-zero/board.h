/*
 * ESP32-S3-Zero board support (a minimal ESP32-S3: no microSD slot, no wired network).
 *
 * This is the S3-Pico stripped further. It runs embedded-only -- the PHP source is packed
 * into a read-only FAT image in flash and mounted by main.c itself -- so this board owns
 * no storage wiring at all. main.c is board-agnostic and talks only to this interface.
 *
 * No microSD: BOARD_HAS_MICROSD is intentionally not defined, so main.c and the discovery
 * firmware skip the SD path entirely, and no board_mount_storage() is provided here.
 *
 * No Ethernet: BOARD_HAS_NETWORK is not defined, so main.c skips the bring-up path and no
 * board_network_up() is provided. The `web-server` model and any `network` features are
 * simply not offered for this board.
 */
#pragma once

#define BOARD_NAME "ESP32-S3-Zero"  /* the specific board (identity for tooling) */
#define BOARD_SOC  "ESP32-S3"       /* the SoC/family, printed in the console banner */
