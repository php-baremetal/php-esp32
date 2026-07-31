cmake_minimum_required(VERSION 3.16)

# --- Board selection -----------------------------------------------------------
# Everything specific to a chip family and a board lives under boards/<family>/<board>/
# (its sdkconfig, pins and board.c). Pick one with -DBOARD=<board> (or <family>/<board>);
# the default comes from `default_board` in php-esp32.toml. Adding a board or a whole
# family is a new directory under boards/, not an edit here.
set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}")
include("${REPO_ROOT}/cmake/resolve-board.cmake")   # -> BOARD, BOARD_DIR, FAMILY_DIR
message(STATUS "Board: ${BOARD}  (${BOARD_DIR})")

# Layer sdkconfig defaults: base (repo root) < family < board. Must be set before
# project() so ESP-IDF reads them (the target and all board config come from here).
set(SDKCONFIG_DEFAULTS
    "${REPO_ROOT}/sdkconfig.defaults"
    "${FAMILY_DIR}/sdkconfig.family"
    "${BOARD_DIR}/sdkconfig.board")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(php-esp32)
