cmake_minimum_required(VERSION 3.16)
# Discovery firmware -- built PER-BOARD, reusing that board's component (board.c), so the peripheral
# probes use the board's real GPIO wiring, not a hardcoded map. phpflash discover --all builds it
# once per candidate board (-DBOARD=<board>) and sees whose peripherals come up.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${REPO_ROOT}/cmake/resolve-board.cmake")   # -> BOARD, BOARD_DIR, FAMILY_DIR
message(STATUS "Discover-fw board: ${BOARD} (${BOARD_DIR})")

# board.cmake / board.c read these in ESP-IDF's early requirement-expansion phase (as in the main
# project, which doesn't see -DBOARD there either).
set(ENV{PHP_ESP32_BOARD} "${BOARD}")
set(ENV{PHP_ESP32_MICROSD} "ON")   # the discovery build always probes the microSD

# Board config (target, chip-rev min, flash size, PSRAM) comes from the layered sdkconfig -- so
# nothing is hardcoded here. Our own file is layered LAST, only to override the partition table:
# the board's custom .csv path is relative to the main project, so here we use a plain single-app
# image (the discovery app is tiny).
set(SDKCONFIG_DEFAULTS
    "${REPO_ROOT}/sdkconfig.defaults"
    "${FAMILY_DIR}/sdkconfig.family"
    "${BOARD_DIR}/sdkconfig.board"
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(discover-fw)
