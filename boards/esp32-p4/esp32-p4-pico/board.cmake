# Build inputs for the ESP32-P4-Pico board. Included by main/CMakeLists.txt, which
# adds these to the main component. CMAKE_CURRENT_LIST_DIR is this board directory.
#
# BOARD_REQUIRES lists the ESP-IDF driver components this board's board.c needs (the
# SD stack here); they end up in the main component's REQUIRES.
set(BOARD_SRCS         "${CMAKE_CURRENT_LIST_DIR}/board.c")
set(BOARD_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}")
set(BOARD_REQUIRES     fatfs vfs esp_driver_sdmmc sdmmc)
