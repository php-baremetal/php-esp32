# Reuse the SELECTED board's component (board.c) so the probe uses that board's real GPIO wiring
# instead of a hardcoded pin map. Same board resolution as the main project's main/CMakeLists.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${REPO_ROOT}/cmake/resolve-board.cmake")   # -> BOARD_DIR
include("${BOARD_DIR}/board.cmake")                 # -> BOARD_SRCS / BOARD_INCLUDE_DIRS / BOARD_REQUIRES

idf_component_register(
    SRCS "discover_main.c" ${BOARD_SRCS}
    INCLUDE_DIRS "." ${BOARD_INCLUDE_DIRS}
    REQUIRES esp_hw_support esp_psram esp_system esp_partition fatfs vfs ${BOARD_REQUIRES}
)

# board.c guards its microSD code with PHP_STORAGE_MICROSD; the discovery build always probes it.
target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_STORAGE_MICROSD=1)
