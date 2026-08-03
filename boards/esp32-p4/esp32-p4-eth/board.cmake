# Build inputs for the ESP32-P4-ETH board. Included by main/CMakeLists.txt, which
# adds these to the main component. CMAKE_CURRENT_LIST_DIR is this board directory.
#
# BOARD_REQUIRES lists the ESP-IDF driver components this board's board.c needs: the SD
# stack, esp_driver_gpio for the GPIO45 SD power-enable, and the Ethernet stack
# (esp_eth + esp_netif + esp_event) for board_network_up().
set(BOARD_SRCS         "${CMAKE_CURRENT_LIST_DIR}/board.c")
set(BOARD_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}")
# Always: fatfs/vfs (the embedded image is FAT too) + the Ethernet stack. The microSD drivers
# (and esp_driver_gpio, used only by the GPIO45 card-power switch) come in only when microSD
# support is compiled (default on) -- read from the env the top-level exports, since the flag
# isn't visible in the early requirement-expansion phase.
set(BOARD_REQUIRES     fatfs vfs esp_eth esp_netif esp_event)
if(NOT "$ENV{PHP_ESP32_MICROSD}" STREQUAL "OFF")
    list(APPEND BOARD_REQUIRES esp_driver_sdmmc sdmmc esp_driver_gpio)
endif()
