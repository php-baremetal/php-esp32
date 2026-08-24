/*
 * ESP32-S3-Zero storage: none of its own.
 *
 * This board is embedded-only -- the PHP source runs from a read-only FAT image in flash,
 * which main.c mounts itself. The board has no microSD slot, so board.h does not define
 * BOARD_HAS_MICROSD and this file provides no board_mount_storage(): nothing references it
 * (main.c and the discovery firmware both gate the SD path on BOARD_HAS_MICROSD).
 *
 * Requesting the card path anyway (`[storage] microsd = true`) is caught at build time by
 * the BOARD_HAS_MICROSD guard in main.c, with a clear message.
 */
#include "board.h"
