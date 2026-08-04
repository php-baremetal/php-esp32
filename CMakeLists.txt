cmake_minimum_required(VERSION 3.16)

# --- Board selection -----------------------------------------------------------
# Everything specific to a chip family and a board lives under boards/<family>/<board>/
# (its sdkconfig, pins and board.c). Pick one with -DBOARD=<board> (or <family>/<board>);
# the default comes from `default_board` in php-esp32.toml. Adding a board or a whole
# family is a new directory under boards/, not an edit here.
set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}")
include("${REPO_ROOT}/cmake/resolve-board.cmake")   # -> BOARD, BOARD_DIR, FAMILY_DIR
message(STATUS "Board: ${BOARD}  (${BOARD_DIR})")

# Export the resolved board so main/CMakeLists can recover it in ESP-IDF's early
# requirement-expansion phase, which doesn't see -DBOARD (see resolve-board.cmake). The
# web-server and microSD flags have the same blind spot (the board's board.cmake reads
# PHP_ESP32_MICROSD to decide whether to pull the SD drivers), so export them too.
set(ENV{PHP_ESP32_BOARD} "${BOARD}")
set(ENV{PHP_ESP32_WEB_SERVER} "${PHP_PROJECT_WEB_SERVER}")
set(ENV{PHP_ESP32_MICROSD} "${PHP_STORAGE_MICROSD}")
set(ENV{PHP_OPENSSL_CONF} "${PHP_OPENSSL_CONF}")

# Layer sdkconfig defaults: base (repo root) < family < board. Must be set before
# project() so ESP-IDF reads them (the target and all board config come from here).
set(SDKCONFIG_DEFAULTS
    "${REPO_ROOT}/sdkconfig.defaults"
    "${FAMILY_DIR}/sdkconfig.family"
    "${BOARD_DIR}/sdkconfig.board")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(php-esp32)

# --- Embedded PHP source (optional) --------------------------------------------
# For "embedded" storage: build a read-only FAT image from PHP_EMBED_SRC (a directory
# holding index.php + vendor/ + ...) and flash it into the `storage` partition, so the
# firmware runs the script from internal flash instead of the microSD. Without this flag
# the partition stays empty and the app falls back to the card (plain microSD mode).
# The image is read-only; a script that needs to write still uses the microSD (mounted
# alongside it at /sdcard).
if(DEFINED PHP_EMBED_SRC AND NOT PHP_EMBED_SRC STREQUAL "")
    if(NOT IS_ABSOLUTE "${PHP_EMBED_SRC}")
        set(PHP_EMBED_SRC "${CMAKE_SOURCE_DIR}/${PHP_EMBED_SRC}")
    endif()
    if(NOT EXISTS "${PHP_EMBED_SRC}")
        message(FATAL_ERROR "PHP_EMBED_SRC='${PHP_EMBED_SRC}' does not exist")
    endif()
    message(STATUS "Embedded PHP source: ${PHP_EMBED_SRC} -> 'storage' partition")
    fatfs_create_rawflash_image(storage "${PHP_EMBED_SRC}" FLASH_IN_PROJECT)
endif()
