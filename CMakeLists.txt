cmake_minimum_required(VERSION 3.16)

# --- Board selection -----------------------------------------------------------
# Everything specific to a chip family and a board lives under boards/<family>/<board>/
# (its sdkconfig, pins and board.c). Pick one with -DBOARD=<board> (or <family>/<board>);
# the default comes from `default_board` in php-esp32.toml. Adding a board or a whole
# family is a new directory under boards/, not an edit here.
set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}")
include("${REPO_ROOT}/cmake/resolve-board.cmake")   # -> BOARD, BOARD_DIR, FAMILY_DIR
message(STATUS "Board: ${BOARD}  (${BOARD_DIR})")

# Export the resolved board so main/CMakeLists can recover it in ESP-IDF's early
# requirement-expansion phase, which doesn't see -DBOARD (see resolve-board.cmake). The
# web-server and microSD flags have the same blind spot (the board's board.cmake reads
# PHP_ESP32_MICROSD to decide whether to pull the SD drivers), so export them too.
set(ENV{PHP_ESP32_BOARD} "${BOARD}")
set(ENV{PHP_ESP32_WEB_SERVER} "${PHP_PROJECT_WEB_SERVER}")
set(ENV{PHP_ESP32_MICROSD} "${PHP_STORAGE_MICROSD}")
set(ENV{PHP_OPENSSL_CONF} "${PHP_OPENSSL_CONF}")
set(ENV{PHP_NET_DNS} "${PHP_NET_DNS}")
set(ENV{PHP_TLS_CAFILE} "${PHP_TLS_CAFILE}")
set(ENV{PHP_ENTRY} "${PHP_ENTRY}")

# Layer sdkconfig defaults: base (repo root) < family < board. Must be set before
# project() so ESP-IDF reads them (the target and all board config come from here).
set(SDKCONFIG_DEFAULTS
    "${REPO_ROOT}/sdkconfig.defaults"
    "${FAMILY_DIR}/sdkconfig.family"
    "${BOARD_DIR}/sdkconfig.board")

# Optional CPU frequency override, from the project config's [board] cpu_freq_mhz (flash-tool passes
# -DPHP_CPU_FREQ_MHZ). Layered LAST so it wins over the board defaults. On the ESP32-P4 the choices
# are 360 (default on rev < 3.0) and 400; 400 on a rev < 3.0 chip is EXPERIMENTAL and needs the force
# flag (Espressif: only chips qualified for 400 MHz -- unqualified ones may be unstable).
if(DEFINED PHP_CPU_FREQ_MHZ AND NOT "${PHP_CPU_FREQ_MHZ}" STREQUAL "")
    set(_cpufreq_frag "${CMAKE_BINARY_DIR}/cpufreq.sdkconfig.defaults")
    set(_cpufreq_body "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_${PHP_CPU_FREQ_MHZ}=y\n")
    if("${PHP_CPU_FREQ_MHZ}" STREQUAL "400")
        string(APPEND _cpufreq_body "CONFIG_ESP_FORCE_400MHZ_ON_REV_LESS_V3=y\n")
    endif()
    file(WRITE "${_cpufreq_frag}" "${_cpufreq_body}")
    list(APPEND SDKCONFIG_DEFAULTS "${_cpufreq_frag}")
    message(STATUS "php-esp32: CPU frequency override -> ${PHP_CPU_FREQ_MHZ} MHz")
endif()

# Pin the ESP-IDF target from the selected family's sdkconfig.family, BEFORE project.cmake.
# ESP-IDF only reads CONFIG_IDF_TARGET out of SDKCONFIG_DEFAULTS *after* it has already fixed
# the target; if IDF_TARGET isn't set it "guesses" from a stray in-source sdkconfig (a leftover
# from an earlier in-tree build), which silently builds the wrong architecture for a different
# family. Setting IDF_TARGET here makes the family's sdkconfig.family the single source of truth
# and stops the guess. FAMILY_DIR comes from resolve-board.cmake.
file(STRINGS "${FAMILY_DIR}/sdkconfig.family" _idf_target_line REGEX "^CONFIG_IDF_TARGET=")
if(_idf_target_line MATCHES "^CONFIG_IDF_TARGET=\"([^\"]+)\"")
    set(IDF_TARGET "${CMAKE_MATCH_1}")
    message(STATUS "php-esp32: IDF target pinned to '${IDF_TARGET}' (from ${FAMILY_DIR}/sdkconfig.family)")
endif()

# Dynamic partition table: the `storage` partition is generated from PHP_EMBED_SRC (present and
# sized to the source for an embedded build, absent for a microSD one) into <project>/build/. Must
# run after SDKCONFIG_DEFAULTS is set and before project(); it appends its own fragment last.
include("${REPO_ROOT}/cmake/gen-partitions.cmake")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(php-esp32)

# --- Embedded PHP source (optional) --------------------------------------------
# For "embedded" storage: build a read-only FAT image from PHP_EMBED_SRC (a directory
# holding index.php + vendor/ + ...) and flash it into the `storage` partition, so the
# firmware runs the script from internal flash instead of the microSD. Without this flag
# the partition stays empty and the app falls back to the card (plain microSD mode).
# The image is read-only; a script that needs to write still uses the microSD (mounted
# alongside it at /sdcard).
if(DEFINED PHP_EMBED_SRC AND NOT PHP_EMBED_SRC STREQUAL "")
    if(NOT IS_ABSOLUTE "${PHP_EMBED_SRC}")
        set(PHP_EMBED_SRC "${CMAKE_SOURCE_DIR}/${PHP_EMBED_SRC}")
    endif()
    if(NOT EXISTS "${PHP_EMBED_SRC}")
        message(FATAL_ERROR "PHP_EMBED_SRC='${PHP_EMBED_SRC}' does not exist")
    endif()
    message(STATUS "Embedded PHP source: ${PHP_EMBED_SRC} -> 'storage' partition")
    fatfs_create_rawflash_image(storage "${PHP_EMBED_SRC}" FLASH_IN_PROJECT)
endif()
