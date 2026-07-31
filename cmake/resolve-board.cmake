# Resolve the selected board to BOARD_DIR / FAMILY_DIR.
#
# Included by both the top-level CMakeLists.txt (to layer the sdkconfig defaults) and
# main/CMakeLists.txt (to include the board's board.cmake) -- the latter runs in
# ESP-IDF's isolated requirement-expansion phase, where variables set by the root are
# NOT visible, so each caller resolves the board itself.
#
# Input: REPO_ROOT (repo root) and, optionally, BOARD (-DBOARD=... or default_board in
# php-esp32.toml). Output: BOARD, BOARD_DIR, FAMILY_DIR.

if(NOT DEFINED REPO_ROOT)
    message(FATAL_ERROR "resolve-board.cmake: REPO_ROOT not set")
endif()

if(NOT DEFINED BOARD OR BOARD STREQUAL "")
    file(STRINGS "${REPO_ROOT}/php-esp32.toml" _board_line REGEX "^[ \t]*default_board")
    string(REGEX MATCH "\"([^\"]+)\"" _ "${_board_line}")
    set(BOARD "${CMAKE_MATCH_1}")
endif()
if(BOARD STREQUAL "")
    message(FATAL_ERROR "No board selected: pass -DBOARD=<board> or set default_board in php-esp32.toml")
endif()

# BOARD may be "family/board" or just "board".
if(BOARD MATCHES "/")
    set(BOARD_DIR "${REPO_ROOT}/boards/${BOARD}")
else()
    file(GLOB _board_matches "${REPO_ROOT}/boards/*/${BOARD}")
    list(LENGTH _board_matches _n)
    if(NOT _n EQUAL 1)
        message(FATAL_ERROR "Board '${BOARD}' not found (or ambiguous) under boards/*/")
    endif()
    list(GET _board_matches 0 BOARD_DIR)
endif()
if(NOT EXISTS "${BOARD_DIR}/board.cmake")
    message(FATAL_ERROR "Unknown board '${BOARD}': ${BOARD_DIR}/board.cmake not found")
endif()
get_filename_component(FAMILY_DIR "${BOARD_DIR}" DIRECTORY)
