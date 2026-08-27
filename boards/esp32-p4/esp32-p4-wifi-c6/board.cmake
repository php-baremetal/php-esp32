# Build inputs for the ESP32-P4-WIFI-C6 board. Included by main/CMakeLists.txt, which
# adds these to the main component. CMAKE_CURRENT_LIST_DIR is this board directory.
#
# BOARD_REQUIRES lists the ESP-IDF driver components this board's board.c needs (the
# SD stack here); they end up in the main component's REQUIRES.
set(BOARD_SRCS         "${CMAKE_CURRENT_LIST_DIR}/board.c")
set(BOARD_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}")
# fatfs/vfs are always needed (the embedded read-only image is FAT too). The microSD drivers
# come in only when microSD support is compiled (default on); the flag isn't visible in the
# early requirement-expansion phase, so we read the env var the top-level CMakeLists exports.
set(BOARD_REQUIRES     fatfs vfs)
if(NOT "$ENV{PHP_ESP32_MICROSD}" STREQUAL "OFF")
    list(APPEND BOARD_REQUIRES esp_driver_sdmmc sdmmc)
endif()
