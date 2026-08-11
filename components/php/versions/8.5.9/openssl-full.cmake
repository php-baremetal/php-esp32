# Full ext/openssl, built against a ported OpenSSL 3.0 libcrypto.
#
# This is the real ext/openssl (openssl.c) -- the complete crypto API: EVP ciphers/digests,
# RSA/EC/DSA/DH keys, X.509 certs and CSRs, PKCS7/PKCS12, sign/verify, seal/open, etc. The TLS
# stream transport (xp_ssl.c) is NOT built (it needs libssl + sockets); its factory is stubbed,
# so ssl:// stream wrappers are absent but every crypto function works. See docs/openssl.md.
#
# libcrypto.a + the OpenSSL headers are produced by scripts/fetch-openssl.sh (git-ignored, like
# the SQLite/oniguruma sources).
set(_ossl_dir "${PHP_COMPONENT_DIR}/openssl-build")
if(NOT EXISTS "${_ossl_dir}/lib/libcrypto.a")
    message(FATAL_ERROR
        "OpenSSL not built: ${_ossl_dir}/lib/libcrypto.a is missing. "
        "Run ./scripts/fetch-openssl.sh first (it downloads and cross-compiles OpenSSL).")
endif()

# Absolute paths: INCLUDE_DIRECTORIES on a source file must be absolute (PHP_SRC is relative).
set(_ext_openssl "${PHP_COMPONENT_DIR}/${PHP_SRC}/ext/openssl")

target_sources(${COMPONENT_LIB} PRIVATE
    "${_ext_openssl}/openssl.c"
    "${_ext_openssl}/openssl_pwhash.c"                   # PHP 8.4: openssl.c calls its PHP_MINIT (Argon2 via EVP_KDF)
    "${_ext_openssl}/openssl_backend_common.c"          # PHP 8.5: backend split (common)
    "${_ext_openssl}/openssl_backend_v3.c"               # PHP 8.5: OpenSSL 3.x backend
    "${PHP_COMPONENT_DIR}/compat/openssl_ssl_stub.c")   # TLS-transport stub + hardware-RNG seed

# openssl.c pulls <openssl/*.h> (the ported headers) and its own ext dir. It also uses the POSIX
# global `timezone` (an ASN.1-time helper); newlib spells it `_timezone` (0 = UTC, which is all
# this target has), so map it. openssl_pwhash.c (8.4) pulls the same ported headers.
set_source_files_properties("${_ext_openssl}/openssl.c" "${_ext_openssl}/openssl_pwhash.c" "${_ext_openssl}/openssl_backend_common.c" "${_ext_openssl}/openssl_backend_v3.c" PROPERTIES
    INCLUDE_DIRECTORIES "${_ext_openssl};${_ossl_dir}/include"
    COMPILE_DEFINITIONS "timezone=_timezone")
set_source_files_properties("${PHP_COMPONENT_DIR}/compat/openssl_ssl_stub.c" PROPERTIES
    INCLUDE_DIRECTORIES "${_ext_openssl};${_ossl_dir}/include")

# HAVE_OPENSSL_EXT activates the extension; PHP_OPENSSL_API_VERSION is derived from the OpenSSL
# version in the headers (3.0 -> 0x30000).
target_compile_definitions(${COMPONENT_LIB} PRIVATE HAVE_OPENSSL_EXT)

# Link the ported libcrypto, and esp_hw_support for esp_fill_random (the RNG method).
target_link_libraries(${COMPONENT_LIB} PRIVATE "${_ossl_dir}/lib/libcrypto.a" idf::esp_hw_support)

# Optional TLS client (-DPHP_EXT_OPENSSL_TLS=ON): compile the real ssl://tls:// transport factory
# backed by esp-tls/mbedTLS instead of the stub, so HTTPS works from PHP. Needs a networked board.
if(PHP_EXT_OPENSSL_TLS)
    target_sources(${COMPONENT_LIB} PRIVATE "${PHP_COMPONENT_DIR}/compat/openssl_tls_esptls.c")
    set_source_files_properties("${PHP_COMPONENT_DIR}/compat/openssl_tls_esptls.c" PROPERTIES
        INCLUDE_DIRECTORIES "${_ext_openssl};${_ossl_dir}/include")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_OPENSSL_TLS)
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::esp-tls)
endif()
