# Resolve the selected board the same way the top-level CMakeLists does -- this file
# also runs in ESP-IDF's isolated requirement-expansion phase, where the root's
# variables aren't visible. The repo root is one level up from main/.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${REPO_ROOT}/cmake/resolve-board.cmake")   # -> BOARD_DIR

# The board provides its sources, include dir and driver requirements.
include("${BOARD_DIR}/board.cmake")

idf_component_register(
    SRCS "main.c" ${BOARD_SRCS}
    INCLUDE_DIRS "." ${BOARD_INCLUDE_DIRS}
    # esp_partition/fatfs/vfs: main.c mounts the optional read-only embedded source
    # partition itself (board-agnostic); the board adds its own storage drivers too.
    REQUIRES php php_ext_gpio esp_partition fatfs vfs ${BOARD_REQUIRES}
)
