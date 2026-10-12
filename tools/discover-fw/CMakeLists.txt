cmake_minimum_required(VERSION 3.16)
# Discovery firmware -- built PER-PRESET, reusing the chip's board.c so the peripheral probes use the
# preset's real GPIO wiring. phpflash discover --all builds it once per candidate preset (passing the
# resolved -DBOARD / -DPHP_PRESET_SDKCONFIG / -DPHP_NETWORK / pins, as the main build does) and sees
# whose peripherals come up.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${REPO_ROOT}/cmake/resolve-board.cmake")   # -> BOARD, CHIP_DIR, MODE_DIR, BOARD_DIR, BOARD_IDF_TARGET
message(STATUS "Discover-fw board: ${BOARD} (${BOARD_DIR})")

# board.cmake / board.c read these in ESP-IDF's early requirement-expansion phase (as in the main
# project, which doesn't see -DBOARD there either). PHP_ESP32_NETWORK is exported by phpflash into the
# process environment, so $ENV{PHP_ESP32_NETWORK} is already visible here.
set(ENV{PHP_ESP32_BOARD} "${BOARD}")
set(ENV{PHP_ESP32_MICROSD} "ON")   # the discovery build always probes the microSD

# Board config (target, chip-rev min, flash size, PSRAM) comes from the layered sdkconfig, mirroring
# the main project: chip + PSRAM mode, then the preset fragment, then our own file LAST (it overrides
# the partition table -- the discovery app is tiny).
set(SDKCONFIG_DEFAULTS "${REPO_ROOT}/sdkconfig.defaults")
list(APPEND SDKCONFIG_DEFAULTS "${CHIP_DIR}/sdkconfig.chip")
if(NOT MODE_DIR STREQUAL CHIP_DIR AND EXISTS "${MODE_DIR}/sdkconfig.chip")
    list(APPEND SDKCONFIG_DEFAULTS "${MODE_DIR}/sdkconfig.chip")
endif()
if(DEFINED PHP_PRESET_SDKCONFIG AND NOT PHP_PRESET_SDKCONFIG STREQUAL "")
    list(APPEND SDKCONFIG_DEFAULTS "${PHP_PRESET_SDKCONFIG}")
endif()
list(APPEND SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")

# Pin the target before project.cmake (as the main project does), so idf.py never guesses it.
if(DEFINED BOARD_IDF_TARGET AND NOT BOARD_IDF_TARGET STREQUAL "")
    set(IDF_TARGET "${BOARD_IDF_TARGET}")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(discover-fw)
