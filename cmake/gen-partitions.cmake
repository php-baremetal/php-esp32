# Dynamic partition table.
#
# The board ships a committed partitions.csv as the spec for the fixed partitions (nvs, phy_init).
# Two partitions are (re)generated here per build instead of being fixed:
#
#   - `factory` (the app): sized to FILL the flash -- flash-tool passes the flash size as
#     PHP_FLASH_SIZE_KB, and factory gets whatever is left after the other partitions. So one layout
#     fits the flash it is built for, and there is no per-board magic number. Without a flash size
#     (back-compat) the board csv's committed factory size is used as a fallback.
#   - `storage` (the read-only FAT image that holds the PHP source for an "embedded" build): sized just
#     above the source. A microSD project gets no storage partition (the app runs from the card).
#
# The generated CSV is written into the project's build/ (never committed) and selected via a
# sdkconfig fragment appended to SDKCONFIG_DEFAULTS -- layered last, so it wins over the board's
# committed CONFIG_PARTITION_TABLE_CUSTOM_FILENAME. Must be included before project().
#
# Inputs: BOARD_DIR, REPO_ROOT (resolve-board.cmake); PHP_EMBED_SRC, PHP_STORAGE_RESERVE_KB,
# PHP_STORE_KB, PHP_FLASH_SIZE_KB, PHP_PARTITIONS_CSV (-D from flash-tool); it appends to
# SDKCONFIG_DEFAULTS in the caller's scope.

# The fixed-partition spec. A project can override the board's committed table by passing
# PHP_PARTITIONS_CSV (flash-tool sets it when a partitions.csv sits next to the project config);
# the generated factory/storage/phpstore partitions below are (re)generated either way.
if(DEFINED PHP_PARTITIONS_CSV AND NOT PHP_PARTITIONS_CSV STREQUAL "")
    set(_board_csv "${PHP_PARTITIONS_CSV}")
    if(NOT IS_ABSOLUTE "${_board_csv}")
        set(_board_csv "${REPO_ROOT}/${_board_csv}")
    endif()
    message(STATUS "php-esp32: partition table base <- project override (${_board_csv})")
else()
    set(_board_csv "${BOARD_DIR}/partitions.csv")
endif()
if(NOT EXISTS "${_board_csv}")
    message(FATAL_ERROR "partition table: ${_board_csv} not found")
endif()

# Keep the board's fixed partitions, but drop `storage` and `factory`: both are (re)generated below --
# factory sized to fill the flash, storage sized to the source. Remember the board's committed factory
# size as the fallback used when the flash size isn't passed.
file(STRINGS "${_board_csv}" _board_lines)
set(_fixed_rows "")
set(_fallback_factory_k 0)
foreach(_line ${_board_lines})
    string(STRIP "${_line}" _stripped)
    if(_stripped STREQUAL "" OR _stripped MATCHES "^#")
        continue()
    endif()
    if(_stripped MATCHES "^storage[ \t,]")
        continue()   # regenerated below
    endif()
    if(_stripped MATCHES "^factory[ \t,]")
        # capture its size (5th CSV field) as the fallback, then drop the line
        string(REPLACE "," ";" _ff "${_stripped}")
        list(LENGTH _ff _fflen)
        if(_fflen GREATER_EQUAL 5)
            list(GET _ff 4 _fsz)
            string(STRIP "${_fsz}" _fsz)
            if(_fsz MATCHES "^([0-9]+)[Kk]$")
                set(_fallback_factory_k "${CMAKE_MATCH_1}")
            elseif(_fsz MATCHES "^([0-9]+)[Mm]$")
                math(EXPR _fallback_factory_k "${CMAKE_MATCH_1} * 1024")
            endif()
        endif()
        continue()
    endif()
    string(APPEND _fixed_rows "${_line}\n")
endforeach()

# storage (embedded source FAT), sized to the source. Empty row + 0 K for a microSD project.
set(_storage_row "")
set(_storage_k 0)
if(DEFINED PHP_EMBED_SRC AND NOT PHP_EMBED_SRC STREQUAL "")
    set(_src "${PHP_EMBED_SRC}")
    if(NOT IS_ABSOLUTE "${_src}")
        set(_src "${REPO_ROOT}/${_src}")
    endif()

    # Cluster-accurate occupancy: fatfsgen uses 4 KB sectors/clusters, so each file rounds up. This
    # counts many small files (a vendor/ tree) correctly instead of undercounting by raw byte sum.
    set(_occ 0)
    file(GLOB_RECURSE _emb_files LIST_DIRECTORIES false CONFIGURE_DEPENDS "${_src}/*")
    foreach(_f ${_emb_files})
        file(SIZE "${_f}" _fs)
        math(EXPR _cl "((${_fs} + 4095) / 4096) * 4096")
        math(EXPR _occ "${_occ} + ${_cl}")
    endforeach()

    if(NOT DEFINED PHP_STORAGE_RESERVE_KB OR "${PHP_STORAGE_RESERVE_KB}" STREQUAL "")
        set(PHP_STORAGE_RESERVE_KB 0)
    endif()
    math(EXPR _reserve_b "${PHP_STORAGE_RESERVE_KB} * 1024")

    # + 64 KB FAT overhead (boot sector, FAT tables, root directory) + the configured reserve;
    # aligned up to 64 KB, floored at 128 KB (fatfsgen needs a valid minimum).
    math(EXPR _size "${_occ} + 65536 + ${_reserve_b}")
    math(EXPR _size "((${_size} + 65535) / 65536) * 65536")
    if(_size LESS 131072)
        set(_size 131072)
    endif()
    math(EXPR _storage_k "${_size} / 1024")

    # Under `secure`, mark the partition `encrypted` (6th CSV column): flash encryption only covers
    # app/bootloader/partition-table by default, so without this the baked PHP source stays in plaintext.
    set(_storage_flags "")
    set(_storage_note "")
    if(PHP_SECURE)
        set(_storage_flags "encrypted")
        set(_storage_note " [encrypted]")
    endif()
    set(_storage_row "storage,    data, fat,      ,         ${_storage_k}K,  ${_storage_flags}\n")
    message(STATUS "php-esp32: embedded 'storage' partition = ${_storage_k}K${_storage_note} "
                   "(source ~${_occ} B occupancy + 64K FAT + ${PHP_STORAGE_RESERVE_KB}K reserve)")
else()
    message(STATUS "php-esp32: microSD project -- no 'storage' partition (source runs from the card)")
endif()

# phpstore (persistent store_* NVS). [store] size_kb -> -DPHP_STORE_KB; absent/0 -> no partition.
set(_phpstore_row "")
set(_phpstore_k 0)
if(DEFINED PHP_STORE_KB AND NOT "${PHP_STORE_KB}" STREQUAL "" AND PHP_STORE_KB GREATER 0)
    math(EXPR _store_b "${PHP_STORE_KB} * 1024")
    math(EXPR _store_b "((${_store_b} + 4095) / 4096) * 4096")   # NVS partitions align to 4 KB
    if(_store_b LESS 16384)
        set(_store_b 16384)                                       # NVS needs a few pages to be useful
    endif()
    math(EXPR _phpstore_k "${_store_b} / 1024")
    set(_phpstore_row "phpstore,   data, nvs,      ,         ${_phpstore_k}K\n")
    message(STATUS "php-esp32: persistent 'phpstore' NVS partition = ${_phpstore_k}K")
endif()

# factory (the app): fill the flash. PHP_FLASH_SIZE_KB (flash-tool, from the board flash size) minus
# the storage/phpstore partitions and a conservative pre-app reserve (bootloader + partition table +
# nvs + phy + alignment; secure boot pushes the app to 0x20000 = 128 K, so 128 K is the safe floor).
set(_pre_k 128)
if(DEFINED PHP_FLASH_SIZE_KB AND NOT "${PHP_FLASH_SIZE_KB}" STREQUAL "" AND PHP_FLASH_SIZE_KB GREATER 0)
    math(EXPR _factory_k "${PHP_FLASH_SIZE_KB} - ${_pre_k} - ${_storage_k} - ${_phpstore_k}")
    math(EXPR _factory_k "(${_factory_k} / 64) * 64")   # app partitions start on a 64 KB boundary
    if(_factory_k LESS 1024)
        message(FATAL_ERROR
            "php-esp32: the project does not fit the board's ${PHP_FLASH_SIZE_KB}K flash -- storage "
            "${_storage_k}K + phpstore ${_phpstore_k}K leave only ${_factory_k}K for the app (need >= 1024K). "
            "Reduce the embedded source / store, or use a board with more flash.")
    endif()
    message(STATUS "php-esp32: 'factory' app partition = ${_factory_k}K (derived to fill ${PHP_FLASH_SIZE_KB}K flash)")
else()
    set(_factory_k "${_fallback_factory_k}")
    if(NOT _factory_k GREATER 0)
        message(FATAL_ERROR "php-esp32: no 'factory' size (flash size not passed and board csv has no factory line).")
    endif()
    message(STATUS "php-esp32: 'factory' app partition = ${_factory_k}K (board fallback; flash size not passed)")
endif()
set(_factory_row "factory,    app,  factory,  ,         ${_factory_k}K\n")

# Assemble in flash order: fixed rows (nvs, phy), then factory, then storage, then phpstore.
set(_csv "# Generated by cmake/gen-partitions.cmake -- do not edit; regenerated each build.\n")
string(APPEND _csv "# Name,     Type, SubType,  Offset,   Size\n")
string(APPEND _csv "${_fixed_rows}")
string(APPEND _csv "${_factory_row}")
string(APPEND _csv "${_storage_row}")
string(APPEND _csv "${_phpstore_row}")

# Write the generated CSV into the project's build/ (the parent of the CMake binary dir, which is
# <project>/build/compiled). Never committed.
get_filename_component(_build_root "${CMAKE_BINARY_DIR}" DIRECTORY)
set(_gen_csv "${_build_root}/partitions.gen.csv")
file(WRITE "${_gen_csv}" "${_csv}")

# Select it: an absolute path in the partition-table filename, layered last over the board default.
set(_part_frag "${CMAKE_BINARY_DIR}/partitions.sdkconfig.defaults")
file(WRITE "${_part_frag}" "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"${_gen_csv}\"\n")
list(APPEND SDKCONFIG_DEFAULTS "${_part_frag}")
message(STATUS "php-esp32: partition table -> ${_gen_csv}")
