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
 * No wired Ethernet: BOARD_HAS_NETWORK is not defined, so main.c skips the wired bring-up path
 * and no board_network_up() is provided. The network instead comes from the S3's built-in Wi-Fi,
 * driven from PHP by the `wifi` extension (join a network or create a SoftAP). The `web-server`
 * model works over that -- the httpd binds regardless of any wired link -- so a project can bring
 * Wi-Fi up in its server_init script and serve pages with no router or cable.
 */
#pragma once

#define BOARD_NAME "ESP32-S3-Zero"  /* the specific board (identity for tooling) */
#define BOARD_SOC  "ESP32-S3"       /* the SoC/family, printed in the console banner */
