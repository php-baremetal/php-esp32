# Build inputs for every ESP32-S3 board. Included by main/CMakeLists.txt (CMAKE_CURRENT_LIST_DIR
# is this chip directory). One board.c serves all S3 boards; the peripheral code inside it is
# compiled only when the project/preset asks for it, and so are the IDF driver components here.
#
# The S3 has no internal SD host nor Ethernet MAC, so both peripherals hang off SPI. The flags are
# read from the environment (exported by the top-level CMakeLists) because -D isn't visible in the
# early requirement-expansion phase where REQUIRES are gathered:
#   PHP_ESP32_MICROSD != OFF -> SD-over-SPI drivers
#   PHP_ESP32_NETWORK == w5500 -> W5500 Ethernet stack
set(BOARD_SRCS         "${CMAKE_CURRENT_LIST_DIR}/board.c")
set(BOARD_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}")
set(BOARD_REQUIRES     fatfs vfs)
if(NOT "$ENV{PHP_ESP32_MICROSD}" STREQUAL "OFF")
    list(APPEND BOARD_REQUIRES esp_driver_spi esp_driver_sdspi sdmmc)
endif()
if("$ENV{PHP_ESP32_NETWORK}" STREQUAL "w5500")
    list(APPEND BOARD_REQUIRES esp_driver_spi esp_eth esp_netif esp_event esp_driver_gpio)
endif()
