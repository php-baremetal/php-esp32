# Build inputs for every ESP32-P4 board. Included by main/CMakeLists.txt (CMAKE_CURRENT_LIST_DIR is
# this chip directory). One board.c serves all P4 boards; the peripheral code inside it is compiled
# only when the project/preset asks for it, and so are the IDF driver components here.
#
# The flags are read from the environment (exported by the top-level CMakeLists) because -D isn't
# visible in the early requirement-expansion phase where REQUIRES are gathered:
#   PHP_ESP32_MICROSD != OFF -> native SDMMC (SDIO) drivers (+ gpio for an optional card-power switch)
#   PHP_ESP32_NETWORK == emac -> internal-EMAC Ethernet stack
# WiFi (PHP_ESP32_NETWORK == wifi-c6) needs no board driver here: the `wifi` extension pulls
# esp_wifi_remote / esp_hosted, and the preset supplies the ESP-HOSTED sdkconfig.
set(BOARD_SRCS         "${CMAKE_CURRENT_LIST_DIR}/board.c")
set(BOARD_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}")
set(BOARD_REQUIRES     fatfs vfs)
if(NOT "$ENV{PHP_ESP32_MICROSD}" STREQUAL "OFF")
    list(APPEND BOARD_REQUIRES esp_driver_sdmmc sdmmc esp_driver_gpio)
endif()
if("$ENV{PHP_ESP32_NETWORK}" STREQUAL "emac")
    list(APPEND BOARD_REQUIRES esp_eth esp_netif esp_event)
endif()
