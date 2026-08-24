# Build inputs for the ESP32-S3-Pico board. Included by main/CMakeLists.txt, which adds
# these to the main component. CMAKE_CURRENT_LIST_DIR is this board directory.
#
# BOARD_REQUIRES lists the ESP-IDF driver components this board's board.c needs. The
# microSD hangs off SPI here (the S3 has no internal SD controller): esp_driver_spi for
# the bus. There is no network hardware on this board, so none of the Ethernet/netif
# stacks are pulled in.
set(BOARD_SRCS         "${CMAKE_CURRENT_LIST_DIR}/board.c")
set(BOARD_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}")
# Always: fatfs/vfs (the embedded image is FAT too) + the SPI bus. The microSD drivers
# (SD-over-SPI: esp_driver_sdspi + the sdmmc protocol layer) come in only when microSD
# support is compiled (default on) -- read from the env the top-level exports, since the
# flag isn't visible in the early requirement-expansion phase.
set(BOARD_REQUIRES     fatfs vfs esp_driver_spi)
if(NOT "$ENV{PHP_ESP32_MICROSD}" STREQUAL "OFF")
    list(APPEND BOARD_REQUIRES esp_driver_sdspi sdmmc)
endif()
