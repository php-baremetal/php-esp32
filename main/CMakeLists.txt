# Resolve the selected board the same way the top-level CMakeLists does -- this file
# also runs in ESP-IDF's isolated requirement-expansion phase, where the root's
# variables aren't visible. The repo root is one level up from main/.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${REPO_ROOT}/cmake/resolve-board.cmake")   # -> BOARD_DIR

# The board provides its sources, include dir and driver requirements.
include("${BOARD_DIR}/board.cmake")

# The `web-server` project type (-DPHP_PROJECT_WEB_SERVER=ON): main.c puts an HTTP server
# (esp_http_server) in front and runs PHP per request. Only then does main need the httpd/net
# components, so they're required conditionally.
# Like -DBOARD, the -D flag isn't visible in the early requirement-expansion phase, so we recover
# it from the environment (exported by the top-level CMakeLists) as a fallback.
option(PHP_PROJECT_WEB_SERVER "Build the web-server execution model (HTTP server + per-request PHP)" OFF)
set(_web_server ${PHP_PROJECT_WEB_SERVER})
if(NOT _web_server AND "$ENV{PHP_ESP32_WEB_SERVER}" STREQUAL "ON")
    set(_web_server ON)
endif()
set(WEB_SERVER_REQUIRES "")
if(_web_server)
    set(WEB_SERVER_REQUIRES esp_http_server esp_netif)
endif()

# microSD support (mount a card for storage / writable data). On by default; a `microsd` project
# needs it, an `embedded` one can turn it off (-DPHP_STORAGE_MICROSD=OFF) to drop the SD drivers.
# The board's board.cmake reads $ENV{PHP_ESP32_MICROSD} to match its REQUIRES in the early phase.
option(PHP_STORAGE_MICROSD "Compile microSD support (mount a card for storage/data)" ON)
set(_microsd ${PHP_STORAGE_MICROSD})
if("$ENV{PHP_ESP32_MICROSD}" STREQUAL "OFF")
    set(_microsd OFF)
endif()

idf_component_register(
    SRCS "main.c" ${BOARD_SRCS}
    INCLUDE_DIRS "." ${BOARD_INCLUDE_DIRS}
    # esp_partition/fatfs/vfs: main.c mounts the optional read-only embedded source
    # partition itself (board-agnostic); the board adds its own storage drivers too.
    REQUIRES php php_ext_gpio esp_partition fatfs vfs ${WEB_SERVER_REQUIRES} ${BOARD_REQUIRES}
)

if(_web_server)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_PROJECT_WEB_SERVER=1)
endif()
if(_microsd)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_STORAGE_MICROSD=1)
endif()

# Path of the openssl.cnf the full openssl build reads (see docs/openssl.md). flash-tool passes
# -DPHP_OPENSSL_CONF from [extensions.openssl] config_path; main.c falls back to "openssl.cnf".
set(_ossl_conf "${PHP_OPENSSL_CONF}")
if(NOT _ossl_conf AND NOT "$ENV{PHP_OPENSSL_CONF}" STREQUAL "")
    set(_ossl_conf "$ENV{PHP_OPENSSL_CONF}")
endif()
if(_ossl_conf)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "PHP_OPENSSL_CONF=\"${_ossl_conf}\"")
endif()

# Static DNS servers (";"-separated) from [network] dns, passed by flash-tool as -DPHP_NET_DNS.
# main.c applies them to the netif after DHCP. Absent -> DHCP-provided DNS stands.
set(_net_dns "${PHP_NET_DNS}")
if(NOT _net_dns AND NOT "$ENV{PHP_NET_DNS}" STREQUAL "")
    set(_net_dns "$ENV{PHP_NET_DNS}")
endif()
if(_net_dns)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "PHP_NET_DNS=\"${_net_dns}\"")
endif()

# CA bundle path for the TLS client, from [extensions.openssl] certs_path (flash-tool passes
# -DPHP_TLS_CAFILE). main.c sets $PHP_TLS_CAFILE so the esp-tls factory verifies peers against it.
set(_tls_ca "${PHP_TLS_CAFILE}")
if(NOT _tls_ca AND NOT "$ENV{PHP_TLS_CAFILE}" STREQUAL "")
    set(_tls_ca "$ENV{PHP_TLS_CAFILE}")
endif()
if(_tls_ca)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "PHP_TLS_CAFILE=\"${_tls_ca}\"")
endif()
