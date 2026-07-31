# php component -- generic, version-selectable.
#
# All version-specific parts (the source list, the hand-written config headers, the
# patches, the optional-extension wiring and the extension manifest) live under
# versions/<PHP_VERSION>/. This file just picks a version and builds it, so adding a
# new PHP version is a new versions/<ver>/ directory, not an edit here.
#
# Select the version with -DPHP_VERSION=<ver> (default below). The default mirrors
# php-esp32.toml at the repo root, which is also what flash-tool reads.

set(PHP_VERSION "8.3.32" CACHE STRING "PHP version to build (needs a versions/<ver> dir)")

set(PHP_COMPONENT_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(PHP_VER_DIR "versions/${PHP_VERSION}")   # relative to the component
set(PHP_SRC "php-${PHP_VERSION}")            # the fetched source tree (git-ignored)

if(NOT EXISTS "${PHP_COMPONENT_DIR}/${PHP_VER_DIR}/sources.cmake")
    message(FATAL_ERROR
        "Unknown PHP version '${PHP_VERSION}': ${PHP_VER_DIR}/sources.cmake not found. "
        "Available: see components/php/versions/.")
endif()
if(NOT EXISTS "${PHP_COMPONENT_DIR}/${PHP_SRC}/main/php_version.h")
    message(FATAL_ERROR
        "PHP source ${PHP_SRC}/ is missing. Run ./scripts/fetch-php.sh first.")
endif()

# The version dir sets PHP_SRCS / PHP_INCLUDE_DIRS / PHP_PRIV_INCLUDE_DIRS.
include("${PHP_COMPONENT_DIR}/${PHP_VER_DIR}/sources.cmake")

idf_component_register(
    SRCS ${PHP_SRCS}
    INCLUDE_DIRS ${PHP_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${PHP_PRIV_INCLUDE_DIRS}
    REQUIRES
        pthread
)

# HAVE_CONFIG_H: use our php_config.h.
# ZEND_ENABLE_STATIC_TSRMLS_CACHE: non-thread-safe build, as in a host embed build.
# KeccakP{200,400,800}_excluded: SHA3 only uses the 1600-bit permutation.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    HAVE_CONFIG_H
    ZEND_ENABLE_STATIC_TSRMLS_CACHE=1
    KeccakP200_excluded
    KeccakP400_excluded
    KeccakP800_excluded
    # newlib defines int32/uint32 via <stdint.h>; stop timelib.h from re-typedef'ing
    # them (uint32_t is unsigned long on this target, not unsigned int).
    HAVE_INT32_T
    HAVE_UINT32_T
    HAVE_INT64_T
    HAVE_UINT64_T
    # getnameinfo constants lwip is missing (dns.c / net.c)
    NI_NUMERICHOST=1
    NI_NUMERICSERV=2
    NI_NOFQDN=4
    NI_NAMEREQD=8
    NI_DGRAM=16
    NI_MAXHOST=1025
    NI_MAXSERV=32
    # lwip has no unix-domain sockets; proc_open.c uses AF_UNIX for socketpair.
    # Compiles this way; socketpair fails at runtime (no processes here anyway).
    AF_UNIX=1
    PF_UNIX=1
)

# -w silences warnings; -fpermissive downgrades the "permerrors" GCC 14 makes hard
# errors by default (incompatible-pointer-types, implicit-function-declaration,
# int-conversion), which -w alone doesn't. This is upstream legacy C.
target_compile_options(${COMPONENT_LIB} PRIVATE -w -fpermissive)

# Optional extensions (version-specific source paths and flags).
include("${PHP_COMPONENT_DIR}/${PHP_VER_DIR}/extensions.cmake")
