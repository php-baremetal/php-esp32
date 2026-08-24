# Build inputs for the ESP32-S3-Zero board. Included by main/CMakeLists.txt, which adds
# these to the main component. CMAKE_CURRENT_LIST_DIR is this board directory.
#
# This board is embedded-only: the PHP source runs from a read-only FAT image in flash,
# so only fatfs/vfs are needed. There is no microSD slot (no SD drivers) and no network
# hardware (no Ethernet/netif stacks).
set(BOARD_SRCS         "${CMAKE_CURRENT_LIST_DIR}/board.c")
set(BOARD_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}")
set(BOARD_REQUIRES     fatfs vfs)
