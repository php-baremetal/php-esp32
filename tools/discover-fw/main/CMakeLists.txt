# Reuse the SELECTED chip's component (board.c) so the probe uses the preset's real GPIO wiring
# instead of a hardcoded pin map. Same board resolution and peripheral gating as the main project.
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

# Wired network type (phpflash passes -DPHP_NETWORK and exports PHP_ESP32_NETWORK): compile board.c's
# bring-up for the matching define so board_network_up() exists and the probe can test the link.
set(_net "${PHP_NETWORK}")
if(NOT _net AND NOT "$ENV{PHP_ESP32_NETWORK}" STREQUAL "")
    set(_net "$ENV{PHP_ESP32_NETWORK}")
endif()
if(_net STREQUAL "w5500")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_NET_W5500=1)
elseif(_net STREQUAL "emac")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_NET_EMAC=1)
endif()

# The preset's display name (phpflash passes -DPHP_BOARD_NAME) -- printed by the probe as board=...
if(DEFINED PHP_BOARD_NAME AND NOT PHP_BOARD_NAME STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "BOARD_NAME=\"${PHP_BOARD_NAME}\"")
endif()

# Peripheral pins a preset configures (flash-tool passes them as -D<name>=<gpio>); board.c falls back
# to its factory wiring when a pin isn't passed.
foreach(_pin PHP_SD_CS PHP_SD_CLK PHP_SD_MOSI PHP_SD_MISO PHP_SD_PWR_EN
             PHP_ETH_CS PHP_ETH_INT PHP_ETH_RST PHP_ETH_SCLK PHP_ETH_MOSI PHP_ETH_MISO)
    if(DEFINED ${_pin} AND NOT "${${_pin}}" STREQUAL "")
        target_compile_definitions(${COMPONENT_LIB} PRIVATE "${_pin}=${${_pin}}")
    endif()
endforeach()
