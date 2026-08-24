/*
 * ESP32-P4-Pico board support.
 *
 * The board owns everything specific to this wiring: the microSD pins, its power
 * (the chip's on-chip LDO), and the mount code in board.c. main.c is board-agnostic
 * and talks only to this interface, so a different board is a new
 * boards/<family>/<board>/ directory implementing the same functions -- even if it
 * wires its storage completely differently (SPI SD, different pins, no LDO).
 */
#pragma once

#include <stdbool.h>

#define BOARD_NAME "ESP32-P4-Pico"   /* the specific board (identity for tooling) */
#define BOARD_SOC  "ESP32-P4"        /* the SoC/family, printed in the console banner */

/* This board has a microSD slot: it provides board_mount_storage(). Boards without a slot
 * (the `-zero` variants) leave this undefined, so the SD path is never built or probed. */
#define BOARD_HAS_MICROSD 1

/* Mount the board's storage at mount_point. On this board that's a 4-bit SDMMC
 * microSD powered by the on-chip LDO (channel 4). Returns true on success. */
bool board_mount_storage(const char *mount_point);

/* Unmount what board_mount_storage() mounted (no-op if nothing is mounted). */
void board_unmount_storage(const char *mount_point);
