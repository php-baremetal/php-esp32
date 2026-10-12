# Resolve the selected board to the directories and the ESP-IDF target the build needs.
#
# Included by both the top-level CMakeLists.txt (to layer the sdkconfig defaults and pin the target)
# and main/CMakeLists.txt (to include the board's board.cmake) -- the latter runs in ESP-IDF's
# isolated requirement-expansion phase, where variables set by the root are NOT visible, so each
# caller resolves the board itself.
#
# Two board models coexist:
#   - NEW (chip -> mode -> preset): BOARD is "<chip>/<mode>" (e.g. "esp32-s3/octal") or just "<chip>"
#     for a mode-less chip. The chip dir holds board.c/board.cmake/chip.toml/sdkconfig.chip; the mode
#     leaf holds sdkconfig.psram. A chip is recognised by a chip.toml.
#   - LEGACY (family -> board): BOARD is a board key (e.g. "esp32-p4-pico"), found by glob under
#     boards/*/<BOARD>, with its sdkconfig.family and (optional) sdkconfig.board.
#
# Input: REPO_ROOT, and optionally BOARD (-DBOARD=... or default_board in php-esp32.toml).
# Output: BOARD, BOARD_DIR (where board.cmake lives), and for the new model NEW_BOARD_MODEL=1 with
# CHIP_DIR/MODE_DIR, for the legacy model FAMILY_DIR; plus BOARD_IDF_TARGET.

if(NOT DEFINED REPO_ROOT)
    message(FATAL_ERROR "resolve-board.cmake: REPO_ROOT not set")
endif()

if(NOT DEFINED BOARD OR BOARD STREQUAL "")
    # -DBOARD is NOT visible in ESP-IDF's early requirement-expansion phase (where this file runs
    # while gathering main's REQUIRES). The top-level CMakeLists exports the resolved board into the
    # environment, which IS visible across the whole cmake process; fall back to that, then to
    # default_board in php-esp32.toml.
    if(NOT "$ENV{PHP_ESP32_BOARD}" STREQUAL "")
        set(BOARD "$ENV{PHP_ESP32_BOARD}")
    else()
        file(STRINGS "${REPO_ROOT}/php-esp32.toml" _board_line REGEX "^[ \t]*default_board")
        string(REGEX MATCH "\"([^\"]+)\"" _ "${_board_line}")
        set(BOARD "${CMAKE_MATCH_1}")
    endif()
endif()
if(BOARD STREQUAL "")
    message(FATAL_ERROR "No board selected: pass -DBOARD=<board> or set default_board in php-esp32.toml")
endif()

# A chip (new model) is the first path segment of BOARD when boards/<segment>/chip.toml exists.
string(REPLACE "/" ";" _board_parts "${BOARD}")
list(GET _board_parts 0 _board_chip)
if(EXISTS "${REPO_ROOT}/boards/${_board_chip}/chip.toml")
    set(NEW_BOARD_MODEL 1)
    set(CHIP_DIR "${REPO_ROOT}/boards/${_board_chip}")
    set(BOARD_DIR "${CHIP_DIR}")            # board.cmake lives at chip level
    list(LENGTH _board_parts _board_nparts)
    if(_board_nparts GREATER 1)
        list(GET _board_parts 1 _board_mode)
        set(MODE_DIR "${CHIP_DIR}/${_board_mode}")
    else()
        set(MODE_DIR "${CHIP_DIR}")         # mode-less chip
    endif()
    # ESP-IDF target from chip.toml (target = "esp32s3").
    file(STRINGS "${CHIP_DIR}/chip.toml" _tgt_line REGEX "^[ \t]*target[ \t]*=")
    if(_tgt_line MATCHES "\"([^\"]+)\"")
        set(BOARD_IDF_TARGET "${CMAKE_MATCH_1}")
    endif()
else()
    set(NEW_BOARD_MODEL 0)
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
    # ESP-IDF target from the family's sdkconfig.family (CONFIG_IDF_TARGET="esp32p4").
    file(STRINGS "${FAMILY_DIR}/sdkconfig.family" _tgt_line REGEX "^CONFIG_IDF_TARGET=")
    if(_tgt_line MATCHES "CONFIG_IDF_TARGET=\"([^\"]+)\"")
        set(BOARD_IDF_TARGET "${CMAKE_MATCH_1}")
    endif()
endif()
