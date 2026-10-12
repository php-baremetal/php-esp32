# Resolve the selected board to the directories and the ESP-IDF target the build needs.
#
# Included by both the top-level CMakeLists.txt (to layer the sdkconfig defaults and pin the target)
# and main/CMakeLists.txt (to include the board's board.cmake) -- the latter runs in ESP-IDF's
# isolated requirement-expansion phase, where variables set by the root are NOT visible, so each
# caller resolves the board itself.
#
# Board model (chip -> mode -> preset): BOARD is "<chip>/<mode>" (e.g. "esp32-s3/octal") or just
# "<chip>" for a mode-less chip. The chip dir holds board.c/board.cmake/chip.toml/sdkconfig.chip; the
# PSRAM-mode leaf holds its own sdkconfig.chip. A chip is recognised by a chip.toml. A product is
# chosen by preset in php-baremetal.config.toml (phpflash writes the preset fragment), not here.
#
# Input: REPO_ROOT, and optionally BOARD (-DBOARD=... or default_board in php-esp32.toml).
# Output: BOARD, CHIP_DIR, MODE_DIR, BOARD_DIR (= CHIP_DIR, where board.cmake lives), BOARD_IDF_TARGET.

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
    message(FATAL_ERROR "No board selected: pass -DBOARD=<chip>[/<mode>] or set default_board in php-esp32.toml")
endif()

# BOARD is "<chip>" or "<chip>/<mode>"; the chip is the first path segment and must carry a chip.toml.
string(REPLACE "/" ";" _board_parts "${BOARD}")
list(GET _board_parts 0 _board_chip)
if(NOT EXISTS "${REPO_ROOT}/boards/${_board_chip}/chip.toml")
    message(FATAL_ERROR
        "Board '${BOARD}' not found. A board is '<chip>' or '<chip>/<mode>' (e.g. esp32-p4, "
        "esp32-s3/octal); a product is chosen by preset in php-baremetal.config.toml. "
        "Moving an older project? See docs/recipes/migrate-config.md.")
endif()
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
