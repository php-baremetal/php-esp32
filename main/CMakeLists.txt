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

# s3_onboard_rgb (ESP32-S3 onboard WS2812 LED): pulled in only when the project enables it.
# The component itself refuses to build on a non-esp32s3 target (see its CMakeLists and the php
# component's extensions.cmake), so this stays empty on the P4.
set(RGB_REQUIRES "")
if(PHP_EXT_S3_ONBOARD_RGB)
    set(RGB_REQUIRES php_ext_s3_onboard_rgb)
endif()

# wifi (STA + SoftAP from PHP): pulled in only when the project enables it. The component refuses
# to build on a radio-less SoC (see its CMakeLists and the php component's extensions.cmake).
set(WIFI_REQUIRES "")
if(PHP_EXT_WIFI)
    set(WIFI_REQUIRES php_ext_wifi)
endif()

# microSD support (mount a card for storage / writable data). On by default; a `microsd` project
# needs it, an `embedded` one can turn it off (-DPHP_STORAGE_MICROSD=OFF) to drop the SD drivers.
# The board's board.cmake reads $ENV{PHP_ESP32_MICROSD} to match its REQUIRES in the early phase.
option(PHP_STORAGE_MICROSD "Compile microSD support (mount a card for storage/data)" ON)
set(_microsd ${PHP_STORAGE_MICROSD})
if("$ENV{PHP_ESP32_MICROSD}" STREQUAL "OFF")
    set(_microsd OFF)
endif()

# Build-time environment: phpflash bakes the project's .env into a generated C file and passes it as
# -DPHP_ENV_SRC; main.c applies it with setenv() so PHP sees it as $_ENV / getenv(). A raw idf.py
# build (no phpflash) gets an empty table so main.c still links.
if(DEFINED PHP_ENV_SRC AND NOT PHP_ENV_SRC STREQUAL "")
    set(_env_src "${PHP_ENV_SRC}")
else()
    set(_env_src "${CMAKE_CURRENT_BINARY_DIR}/php_env.empty.c")
    file(WRITE "${_env_src}"
        "const char *const php_esp32_env[] = {(const char *)0};\nconst int php_esp32_env_count = 0;\n")
endif()

idf_component_register(
    SRCS "main.c" "fs_pathnorm.c" "fs_glob.c" "${_env_src}" ${BOARD_SRCS}
    INCLUDE_DIRS "." ${BOARD_INCLUDE_DIRS}
    # esp_partition/fatfs/vfs: main.c mounts the optional read-only embedded source
    # partition itself (board-agnostic); the board adds its own storage drivers too.
    REQUIRES php php_ext_gpio php_ext_store php_ext_mem php_ext_baremetal_utility php_project_exts esp_partition esp_app_format fatfs vfs ${WEB_SERVER_REQUIRES} ${RGB_REQUIRES} ${WIFI_REQUIRES} ${BOARD_REQUIRES}
)

# fs_pathnorm.c: --wrap the file syscalls so "." / ".." in paths are collapsed before the FATFS VFS
# (which can't resolve them). Global link options so the wrap applies to every caller (PHP included).
foreach(_fn stat lstat open opendir access mkdir unlink rmdir rename)
    idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=${_fn}" APPEND)
endforeach()

# glob()/globfree() (fs_glob.c): the C library's glob() fails on the FATFS VFS, so we provide a
# readdir()-based one. Symfony needs glob() to discover its service classes. The -u forces the linker
# to pull fs_glob.o from libmain.a (few callers reference glob, so it isn't pulled on demand in time).
foreach(_fn glob globfree)
    idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=${_fn}" APPEND)
    idf_build_set_property(LINK_OPTIONS "-Wl,-u,__wrap_${_fn}" APPEND)
endforeach()

if(_web_server)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_PROJECT_WEB_SERVER=1)
endif()
if(_microsd)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_STORAGE_MICROSD=1)
endif()

# OPcache (-DPHP_EXT_OPCACHE=ON): main.c installs the embed SAPI ini_defaults hook that turns it on
# and points its file cache at the card. The extension itself is built in the php component; here we
# just let main.c know it's present. Recovered from the environment in the early phase like the flags
# above (the top-level CMakeLists exports it).
if(PHP_EXT_OPCACHE OR "$ENV{PHP_ESP32_OPCACHE}" STREQUAL "ON")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_OPCACHE_ENABLED=1)
endif()
# OPcache `in_memory` setting (-DPHP_EXT_OPCACHE_SHM=ON): main.c seeds the SHM (PSRAM) ini instead of
# the microSD file cache. Read from the cache var flash-tool passes; not declared as an option() here
# (that's reserved for project-type flags), only in the php component's extensions.cmake.
if(PHP_EXT_OPCACHE_SHM)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_OPCACHE_SHM_MODE=1)
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

# Entry script within the source ([php] entry, default index.php), passed by flash-tool as
# -DPHP_ENTRY. Lets a framework with a nested front controller run (Laravel: public/index.php).
set(_php_entry "${PHP_ENTRY}")
if(NOT _php_entry AND NOT "$ENV{PHP_ENTRY}" STREQUAL "")
    set(_php_entry "$ENV{PHP_ENTRY}")
endif()
if(_php_entry)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "PHP_ENTRY=\"${_php_entry}\"")
endif()

# -DPHP_WEB_INIT from [web-server] init: a one-time script the web-server model runs once before the
# HTTP server starts (main.c resolves it against the source mount). Only meaningful for web-server.
set(_web_init "${PHP_WEB_INIT}")
if(NOT _web_init AND NOT "$ENV{PHP_WEB_INIT}" STREQUAL "")
    set(_web_init "$ENV{PHP_WEB_INIT}")
endif()
if(_web_init)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "PHP_WEB_INIT=\"${_web_init}\"")
endif()

# -DPHP_ESP32_PROJECT_NAME from the project config's `name` (flash-tool passes it): the firmware
# exposes it to phpinfo()'s "PHP Baremetal Infos" table. Recover it from the env in the early
# requirement-expansion pass too, like the flags above.
set(_proj_name "${PHP_ESP32_PROJECT_NAME}")
if(NOT _proj_name AND NOT "$ENV{PHP_ESP32_PROJECT_NAME}" STREQUAL "")
    set(_proj_name "$ENV{PHP_ESP32_PROJECT_NAME}")
endif()
if(_proj_name)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "PHP_ESP32_PROJECT_NAME=\"${_proj_name}\"")
endif()
