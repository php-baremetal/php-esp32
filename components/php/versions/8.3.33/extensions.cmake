# Optional-extension wiring for PHP 8.3.33. Included after idf_component_register
# by the generic CMakeLists.txt; uses ${COMPONENT_LIB}, ${PHP_SRC},
# ${PHP_VER_DIR} and ${PHP_COMPONENT_DIR}.

# --- Optional extensions -----------------------------------------------------
# Kept out of the default build; each is gated by a -DPHP_EXT_<NAME>=ON flag that
# flash.sh passes to idf.py. The sources come from the (git-ignored) vendored
# trees, so nothing here changes a plain build. To add another optional extension
# later, mirror this block and add a matching guard in internal_functions.c.

# ext/date: the real DateTime and date/time functions. Off by default -- the core
# normally uses compat/date_stub.c (a UTC-only stub) so php_time()/php_format_date()
# resolve. When ON, the real timelib provides those, so the stub must NOT be built
# (it would be a duplicate symbol). Flash cost ~650 KB, mostly the ~350 KB builtin
# timezone database. Enable with idf.py -DPHP_EXT_DATE=ON (or "y" in flash.sh).
option(PHP_EXT_DATE "Build the real ext/date (DateTime) instead of the UTC stub" OFF)
# With PHP_EXT_DATE, ship only UTC (compat/timezonedb_minimal.h, ~2.7 KB) instead of
# the full builtin timezone database (~350 KB). DateTime then works in UTC only; named
# zones report an error. Relies on the parse_tz.c patch (patches/php/0002-...).
option(PHP_EXT_DATE_MINIMAL_TZ "With PHP_EXT_DATE: UTC-only timezone db, no named zones" OFF)
if(PHP_EXT_DATE)
    target_sources(${COMPONENT_LIB} PRIVATE
        ${PHP_SRC}/ext/date/php_date.c
        ${PHP_SRC}/ext/date/lib/astro.c
        ${PHP_SRC}/ext/date/lib/dow.c
        ${PHP_SRC}/ext/date/lib/interval.c
        ${PHP_SRC}/ext/date/lib/parse_date.c
        ${PHP_SRC}/ext/date/lib/parse_iso_intervals.c
        ${PHP_SRC}/ext/date/lib/parse_posix.c
        ${PHP_SRC}/ext/date/lib/parse_tz.c
        ${PHP_SRC}/ext/date/lib/timelib.c
        ${PHP_SRC}/ext/date/lib/tm2unixtime.c
        ${PHP_SRC}/ext/date/lib/unixtime2tm.c
    )
    target_include_directories(${COMPONENT_LIB} PRIVATE
        ${PHP_SRC}/ext/date
        ${PHP_SRC}/ext/date/lib
    )
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_DATE_ENABLED)
    if(PHP_EXT_DATE_MINIMAL_TZ)
        # parse_tz.c (patched) picks up compat/timezonedb_minimal.h, already on the path.
        target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_DATE_MINIMAL_TZ)
    endif()
else()
    target_sources(${COMPONENT_LIB} PRIVATE ${PHP_VER_DIR}/compat/date_stub.c)
endif()

# PDO + SQLite: read/write .db files on the microSD. Enable with:
#   idf.py -DPHP_EXT_SQLITE=ON ...   (or answer "y" in flash.sh)
# Needs the SQLite amalgamation: ./scripts/fetch-sqlite.sh
option(PHP_EXT_SQLITE "Build the PDO/SQLite extension" OFF)
if(PHP_EXT_SQLITE)
    if(NOT EXISTS "${PHP_COMPONENT_DIR}/sqlite-amalgamation/sqlite3.c")
        message(FATAL_ERROR
            "PHP_EXT_SQLITE=ON but the SQLite amalgamation is missing. "
            "Run ./scripts/fetch-sqlite.sh first.")
    endif()
    target_sources(${COMPONENT_LIB} PRIVATE
        ${PHP_SRC}/ext/pdo/pdo.c
        ${PHP_SRC}/ext/pdo/pdo_dbh.c
        ${PHP_SRC}/ext/pdo/pdo_sql_parser.c
        ${PHP_SRC}/ext/pdo/pdo_sqlstate.c
        ${PHP_SRC}/ext/pdo/pdo_stmt.c
        ${PHP_SRC}/ext/pdo_sqlite/pdo_sqlite.c
        ${PHP_SRC}/ext/pdo_sqlite/sqlite_driver.c
        ${PHP_SRC}/ext/pdo_sqlite/sqlite_statement.c
        sqlite-amalgamation/sqlite3.c
    )
    target_include_directories(${COMPONENT_LIB} PRIVATE
        sqlite-amalgamation
        ${PHP_SRC}/ext
    )
    # Force-include a small shim into sqlite3.c only: it neutralizes the project's
    # global HAVE_*INT*_T macros (which would misconfigure SQLite's integer
    # typedefs) and maps lstat->stat. Scoped to this file so nothing else changes.
    set_source_files_properties(sqlite-amalgamation/sqlite3.c PROPERTIES
        COMPILE_OPTIONS "-include;${PHP_COMPONENT_DIR}/compat/sqlite-compat.h")
    # PHP_EXT_SQLITE_ENABLED gates the module registration in internal_functions.c.
    # The SQLite flags tune it for a single-process, no-OS target on FATFS: no
    # threads, no WAL, no mmap, temp tables in RAM, and URI filenames (so the DSN
    # can pass ?nolock=1 to skip POSIX file locking, which FATFS doesn't provide).
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        PHP_EXT_SQLITE_ENABLED
        SQLITE_THREADSAFE=0
        SQLITE_OMIT_WAL
        SQLITE_OMIT_LOAD_EXTENSION
        SQLITE_TEMP_STORE=3
        SQLITE_MAX_MMAP_SIZE=0
        SQLITE_DEFAULT_MEMSTATUS=0
        SQLITE_USE_URI=1
    )
endif()

# ext/ctype: the ctype_*() character-class checks. Tiny (one source file, no data
# tables). Pulled in by doctrine/inflector, Carbon and illuminate/support.
# Enable with idf.py -DPHP_EXT_CTYPE=ON (or "y" in flash.sh).
option(PHP_EXT_CTYPE "Build the ext/ctype extension" OFF)
if(PHP_EXT_CTYPE)
    target_sources(${COMPONENT_LIB} PRIVATE ${PHP_SRC}/ext/ctype/ctype.c)
    target_include_directories(${COMPONENT_LIB} PRIVATE ${PHP_SRC}/ext/ctype)
    # ctype.c wraps its whole body (module entry included) in #ifdef HAVE_CTYPE,
    # which configure would define. Scope it to this one file so nothing else sees it.
    set_source_files_properties(${PHP_SRC}/ext/ctype/ctype.c PROPERTIES
        COMPILE_DEFINITIONS "HAVE_CTYPE")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_CTYPE_ENABLED)
endif()

# ext/filter: filter_var() validation and sanitization. Depends on ext/pcre (built
# in) and ext/standard.
# Enable with idf.py -DPHP_EXT_FILTER=ON (or "y" in flash.sh).
option(PHP_EXT_FILTER "Build the ext/filter extension" OFF)
if(PHP_EXT_FILTER)
    target_sources(${COMPONENT_LIB} PRIVATE
        ${PHP_SRC}/ext/filter/filter.c
        ${PHP_SRC}/ext/filter/logical_filters.c
        ${PHP_SRC}/ext/filter/sanitizing_filters.c
        ${PHP_SRC}/ext/filter/callback_filter.c
    )
    target_include_directories(${COMPONENT_LIB} PRIVATE ${PHP_SRC}/ext/filter)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_FILTER_ENABLED)
endif()

# ext/tokenizer: token_get_all() / PhpToken over the Zend lexer (already in the engine).
# Self-contained -- just its two source files (tokenizer_data.c ships pre-generated).
# Enable with idf.py -DPHP_EXT_TOKENIZER=ON (or "y" in flash.sh).
option(PHP_EXT_TOKENIZER "Build the ext/tokenizer extension" OFF)
if(PHP_EXT_TOKENIZER)
    target_sources(${COMPONENT_LIB} PRIVATE
        ${PHP_SRC}/ext/tokenizer/tokenizer.c
        ${PHP_SRC}/ext/tokenizer/tokenizer_data.c
    )
    target_include_directories(${COMPONENT_LIB} PRIVATE ${PHP_SRC}/ext/tokenizer)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_TOKENIZER_ENABLED)
endif()

# ext/session: session_start()/$_SESSION with the default "files" save handler (writes to a
# writable dir -- the microSD; set session.save_path from PHP) plus the user save handler
# (session_set_save_handler). mod_mm.c (shared-memory handler) is HAVE_LIBMM-gated and skipped.
# Leans on ext/standard (url_scanner, flock_compat -- both always built) and ext/hash.
# Enable with idf.py -DPHP_EXT_SESSION=ON (or "y" in flash.sh).
option(PHP_EXT_SESSION "Build the ext/session extension" OFF)
if(PHP_EXT_SESSION)
    target_sources(${COMPONENT_LIB} PRIVATE
        ${PHP_SRC}/ext/session/session.c
        ${PHP_SRC}/ext/session/mod_files.c
        ${PHP_SRC}/ext/session/mod_user.c
        ${PHP_SRC}/ext/session/mod_user_class.c
    )
    target_include_directories(${COMPONENT_LIB} PRIVATE ${PHP_SRC}/ext/session)
    # HAVE_PHP_SESSION is the "session is available" signal other code uses to integrate with it.
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_SESSION_ENABLED HAVE_PHP_SESSION)
endif()

# ext/mbstring: the mb_*() multibyte-string functions plus the bundled libmbfl
# charset library. Built WITHOUT oniguruma, so the mb_ereg* regex family is left
# out (php_mbregex.c is not compiled and HAVE_MBREGEX stays undefined) -- the rest
# of mbstring (mb_strlen/substr/convert_encoding/detect_encoding/...) is all there.
# This is the heavy one: libmbfl carries the CJK conversion tables (~740 KB of it).
# Enable with idf.py -DPHP_EXT_MBSTRING=ON.
option(PHP_EXT_MBSTRING "Build the ext/mbstring extension (no mb_ereg/oniguruma)" OFF)
# Sub-option: drop the legacy CJK (Chinese/Japanese/Korean) encodings. Leaves out
# mbfilter_cjk.c (and mbfilter_utf8_mobile.c, which reuses its emoji tables) and, via
# the 0003 patch, their registry entries. Shrinks mbstring by ~740 KB; UTF-8/UTF-16/
# Latin and everything else keep working. No named CJK codecs (Shift-JIS, EUC-*, Big5).
option(PHP_EXT_MBSTRING_NO_CJK "With PHP_EXT_MBSTRING: drop the CJK encodings (~740 KB smaller)" OFF)
# Sub-option: build the mb_ereg*/mb_split multibyte-regex family. PHP does not bundle its
# regex engine (oniguruma), so this vendors it -- run ./scripts/fetch-oniguruma.sh first.
# Off by default; only needed by code that calls mb_ereg* or mb_split.
option(PHP_EXT_MBSTRING_ONIG "With PHP_EXT_MBSTRING: build mb_ereg*/mb_split (bundles oniguruma)" OFF)
if(PHP_EXT_MBSTRING)
    # mbstring.c does #include "libmbfl/config.h", a file configure would generate to
    # pull in the autoconf defines. We don't run configure, so generate a one-line shim
    # in the build tree (it just includes our hand-written php_config.h) and put its
    # parent on the include path so the quoted include resolves.
    set(_mbfl_cfg_dir "${CMAKE_CURRENT_BINARY_DIR}/mbstring-generated")
    file(MAKE_DIRECTORY "${_mbfl_cfg_dir}/libmbfl")
    file(WRITE "${_mbfl_cfg_dir}/libmbfl/config.h"
        "/* generated: libmbfl pulls in the project's php_config.h */\n#include <php_config.h>\n")
    target_sources(${COMPONENT_LIB} PRIVATE
        ${PHP_SRC}/ext/mbstring/mbstring.c
        ${PHP_SRC}/ext/mbstring/php_unicode.c
        ${PHP_SRC}/ext/mbstring/mb_gpc.c
        # bundled libmbfl (charset conversion) -- the whole library, minus nothing.
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/html_entities.c
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_7bit.c
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_base64.c
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_htmlent.c
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_qprint.c
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_singlebyte.c
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_ucs2.c
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_ucs4.c
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_utf16.c
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_utf32.c
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_utf7.c
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_utf7imap.c
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_utf8.c
        ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_uuencode.c
        ${PHP_SRC}/ext/mbstring/libmbfl/mbfl/mbfilter_8bit.c
        ${PHP_SRC}/ext/mbstring/libmbfl/mbfl/mbfilter.c
        ${PHP_SRC}/ext/mbstring/libmbfl/mbfl/mbfilter_pass.c
        ${PHP_SRC}/ext/mbstring/libmbfl/mbfl/mbfilter_wchar.c
        ${PHP_SRC}/ext/mbstring/libmbfl/mbfl/mbfl_convert.c
        ${PHP_SRC}/ext/mbstring/libmbfl/mbfl/mbfl_encoding.c
        ${PHP_SRC}/ext/mbstring/libmbfl/mbfl/mbfl_filter_output.c
        ${PHP_SRC}/ext/mbstring/libmbfl/mbfl/mbfl_language.c
        ${PHP_SRC}/ext/mbstring/libmbfl/mbfl/mbfl_memory_device.c
        ${PHP_SRC}/ext/mbstring/libmbfl/mbfl/mbfl_string.c
        ${PHP_SRC}/ext/mbstring/libmbfl/nls/nls_de.c
        ${PHP_SRC}/ext/mbstring/libmbfl/nls/nls_en.c
        ${PHP_SRC}/ext/mbstring/libmbfl/nls/nls_hy.c
        ${PHP_SRC}/ext/mbstring/libmbfl/nls/nls_ja.c
        ${PHP_SRC}/ext/mbstring/libmbfl/nls/nls_kr.c
        ${PHP_SRC}/ext/mbstring/libmbfl/nls/nls_neutral.c
        ${PHP_SRC}/ext/mbstring/libmbfl/nls/nls_ru.c
        ${PHP_SRC}/ext/mbstring/libmbfl/nls/nls_tr.c
        ${PHP_SRC}/ext/mbstring/libmbfl/nls/nls_ua.c
        ${PHP_SRC}/ext/mbstring/libmbfl/nls/nls_uni.c
        ${PHP_SRC}/ext/mbstring/libmbfl/nls/nls_zh.c
    )
    # The legacy CJK codecs live in mbfilter_cjk.c (+ mbfilter_utf8_mobile.c, which reuses
    # its emoji tables) -- ~740 KB. Make them optional: with PHP_EXT_MBSTRING_NO_CJK they're
    # dropped and the 0003 patch removes their registry entries (guarded by MBSTRING_NO_CJK)
    # so nothing references the missing symbols.
    if(NOT PHP_EXT_MBSTRING_NO_CJK)
        target_sources(${COMPONENT_LIB} PRIVATE
            ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_cjk.c
            ${PHP_SRC}/ext/mbstring/libmbfl/filters/mbfilter_utf8_mobile.c
        )
    else()
        target_compile_definitions(${COMPONENT_LIB} PRIVATE MBSTRING_NO_CJK)
    endif()
    target_include_directories(${COMPONENT_LIB} PRIVATE
        ${_mbfl_cfg_dir}
        ${PHP_SRC}/ext/mbstring
        ${PHP_SRC}/ext/mbstring/libmbfl
        ${PHP_SRC}/ext/mbstring/libmbfl/mbfl
    )
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_MBSTRING_ENABLED)

    # mb_ereg*/mb_split (multibyte regex). PHP doesn't bundle its regex engine, so this
    # compiles the vendored oniguruma (git-ignored: ./scripts/fetch-oniguruma.sh) plus
    # php_mbregex.c, and defines HAVE_MBREGEX so mbstring.c and its arginfo register the
    # mb_ereg* / mb_split functions. Without this the family simply isn't there.
    if(PHP_EXT_MBSTRING_ONIG)
        if(NOT EXISTS "${PHP_COMPONENT_DIR}/oniguruma/src/oniguruma.h")
            message(FATAL_ERROR
                "PHP_EXT_MBSTRING_ONIG=ON but oniguruma is missing. "
                "Run ./scripts/fetch-oniguruma.sh first.")
        endif()
        # libonig sources (the Makefile.am list, minus the POSIX API wrappers and mktable).
        # The onig .c files find their own headers and src/config.h relative to src/, so
        # they need no extra include dirs.
        set(_onig src/regparse.c src/regcomp.c src/regexec.c src/regenc.c src/regerror.c
            src/regext.c src/regsyntax.c src/regtrav.c src/regversion.c src/st.c src/reggnu.c
            src/unicode.c src/unicode_unfold_key.c src/unicode_fold1_key.c
            src/unicode_fold2_key.c src/unicode_fold3_key.c src/ascii.c src/utf8.c
            src/utf16_be.c src/utf16_le.c src/utf32_be.c src/utf32_le.c src/euc_jp.c
            src/euc_jp_prop.c src/sjis.c src/sjis_prop.c src/iso8859_1.c src/iso8859_2.c
            src/iso8859_3.c src/iso8859_4.c src/iso8859_5.c src/iso8859_6.c src/iso8859_7.c
            src/iso8859_8.c src/iso8859_9.c src/iso8859_10.c src/iso8859_11.c src/iso8859_13.c
            src/iso8859_14.c src/iso8859_15.c src/iso8859_16.c src/euc_tw.c src/euc_kr.c
            src/big5.c src/gb18030.c src/koi8.c src/koi8_r.c src/cp1251.c src/onig_init.c)
        list(TRANSFORM _onig PREPEND oniguruma/)
        target_sources(${COMPONENT_LIB} PRIVATE
            ${PHP_SRC}/ext/mbstring/php_mbregex.c
            ${_onig}
        )
        # HAVE_MBREGEX gates the mb_ereg* code in mbstring.c and its arginfo entries.
        target_compile_definitions(${COMPONENT_LIB} PRIVATE HAVE_MBREGEX)
        # php_mbregex.c pulls <oniguruma.h> and needs ONIG_ESCAPE_UCHAR_COLLISION so
        # oniguruma.h doesn't re-typedef UChar (php.h already has it). Scoped to that file
        # so the include of oniguruma/src doesn't leak a bare "config.h" onto other units.
        set_source_files_properties(${PHP_SRC}/ext/mbstring/php_mbregex.c PROPERTIES
            INCLUDE_DIRECTORIES "${PHP_COMPONENT_DIR}/oniguruma/src"
            COMPILE_DEFINITIONS "ONIG_ESCAPE_UCHAR_COLLISION=1")
    endif()
endif()

# --- openssl (optional) -------------------------------------------------------
# The `openssl` extension comes in two flavours; a project picks one:
#   PHP_EXT_OPENSSL             a small mbedTLS-backed *compatible subset* (symmetric AES only:
#                              openssl_encrypt/decrypt, iv_length, random_pseudo_bytes). No RSA/
#                              X.509/TLS. Enough for e.g. Laravel's Encrypter. Adds ~tens of KB.
#   + PHP_EXT_OPENSSL_FULL      instead build the *real* ext/openssl against a ported OpenSSL
#                              library -- the full API, but large (~MBs). See docs/openssl.md.
# Both register the same `openssl` module, so exactly one is compiled.
option(PHP_EXT_OPENSSL "Build the openssl extension (mbedTLS-backed compatible subset by default)" OFF)
option(PHP_EXT_OPENSSL_FULL "With PHP_EXT_OPENSSL: build the real ext/openssl on a ported OpenSSL (full API, large)" OFF)
# Full only: skip loading openssl.cnf at startup (OPENSSL_INIT_NO_LOAD_CONFIG) instead of reading
# one via OPENSSL_CONF. Off by default -- see docs/openssl.md for when to turn it on.
option(PHP_EXT_OPENSSL_NO_LOAD_CONFIG "With PHP_EXT_OPENSSL_FULL: don't load openssl.cnf (skip config)" OFF)
# Full only: build the real ssl://tls:// stream transport (esp-tls/mbedTLS backed) so PHP can do
# HTTPS. Needs a networked board. Off by default -- see docs/openssl.md.
option(PHP_EXT_OPENSSL_TLS "With PHP_EXT_OPENSSL_FULL: build the TLS client transport (HTTPS) on esp-tls" OFF)
if(PHP_EXT_OPENSSL)
    if(PHP_EXT_OPENSSL_FULL)
        include("${PHP_COMPONENT_DIR}/${PHP_VER_DIR}/openssl-full.cmake")
        if(PHP_EXT_OPENSSL_NO_LOAD_CONFIG)
            target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_OPENSSL_NO_LOAD_CONFIG)
        endif()
    else()
        # Compatible subset: our hand-written extension on ESP-IDF's mbedTLS + hardware RNG.
        target_sources(${COMPONENT_LIB} PRIVATE "${PHP_COMPONENT_DIR}/compat/openssl_compat.c")
        target_link_libraries(${COMPONENT_LIB} PRIVATE idf::mbedtls idf::esp_hw_support)
    endif()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_OPENSSL_ENABLED)
endif()

# --- opcache (optional) -------------------------------------------------------
# Zend OPcache, statically linked (there is no opcache.so to dlopen on this target). Built WITHOUT
# JIT (unsupported on RISC-V) and driven in `file_cache_only` mode: compiled bytecode is cached to
# a directory (opcache.file_cache) instead of shared memory, so no SHM/mmap is needed. The firmware
# (main.c) turns it on via the embed SAPI's ini_defaults hook, and patch 0006 registers its
# zend_extension + teaches accel_find_sapi() the "embed" SAPI. See docs/opcache.md.
option(PHP_EXT_OPCACHE "Build Zend OPcache (bytecode cache; no JIT)" OFF)
# The `in_memory` setting: keep the cache in PSRAM (SHM) instead of the microSD file cache. The
# backend (shared_alloc_malloc.c) is always compiled; main.c reads this flag to pick the ini mode.
# Only meaningful with PHP_EXT_OPCACHE.
option(PHP_EXT_OPCACHE_SHM "OPcache in_memory: keep the bytecode cache in PSRAM (SHM) not on the card" OFF)
if(PHP_EXT_OPCACHE)
    target_sources(${COMPONENT_LIB} PRIVATE
        ${PHP_SRC}/ext/opcache/ZendAccelerator.c
        ${PHP_SRC}/ext/opcache/zend_accelerator_blacklist.c
        ${PHP_SRC}/ext/opcache/zend_accelerator_debug.c
        ${PHP_SRC}/ext/opcache/zend_accelerator_hash.c
        ${PHP_SRC}/ext/opcache/zend_accelerator_module.c
        ${PHP_SRC}/ext/opcache/zend_persist.c
        ${PHP_SRC}/ext/opcache/zend_persist_calc.c
        ${PHP_SRC}/ext/opcache/zend_file_cache.c
        ${PHP_SRC}/ext/opcache/zend_shared_alloc.c
        ${PHP_SRC}/ext/opcache/zend_accelerator_util_funcs.c
        ${PHP_SRC}/ext/opcache/shared_alloc_shm.c
        ${PHP_SRC}/ext/opcache/shared_alloc_mmap.c
        ${PHP_SRC}/ext/opcache/shared_alloc_posix.c
        # Weak no-op POSIX symbols (mmap/shm*) the SHM backends reference but never call here.
        ${PHP_COMPONENT_DIR}/compat/opcache_posix_stubs.c
        # Heap-backed shared-memory backend (USE_MALLOC_SHM): keeps the bytecode cache in PSRAM
        # across requests, instead of re-reading it from the file cache each time.
        ${PHP_COMPONENT_DIR}/compat/shared_alloc_malloc.c
    )
    # opcache_stubs/ supplies sys/ipc.h, sys/shm.h, sys/mman.h -- headers picolibc lacks. They are
    # missing system-wide and no other source includes them, so this only affects the opcache files.
    target_include_directories(${COMPONENT_LIB} PRIVATE
        ${PHP_SRC}/ext/opcache
        ${PHP_COMPONENT_DIR}/compat/opcache_stubs
    )
    # PHP_EXT_OPCACHE_ENABLED: our marker (patch 0006 keys the zend_extension registration on it).
    # ZEND_ENABLE_STATIC_TSRMLS_CACHE=1: opcache's required build flag. HAVE_JIT is left undefined
    # (no JIT); the shared_alloc_*.c backends compile empty without USE_MMAP/USE_SHM* and are never
    # used in file_cache_only mode.
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        PHP_EXT_OPCACHE_ENABLED
        ZEND_ENABLE_STATIC_TSRMLS_CACHE=1
        USE_MALLOC_SHM   # register the PSRAM shared-memory backend (shared_alloc_malloc.c)
    )
endif()

# --- s3_onboard_rgb: ESP32-S3 onboard WS2812 RGB LED (S3-only) -----------------
# The extension itself lives in components/php_ext_s3_onboard_rgb/. Here we gate the
# target and switch on its registration in internal_functions.c.
option(PHP_EXT_S3_ONBOARD_RGB "Build the s3_onboard_rgb extension (ESP32-S3 onboard RGB LED)" OFF)
if(PHP_EXT_S3_ONBOARD_RGB)
    if(NOT IDF_TARGET STREQUAL "esp32s3")
        message(FATAL_ERROR "PHP_EXT_S3_ONBOARD_RGB is ESP32-S3 only: the onboard RGB LED is an ESP32-S3 board feature, but the target is '${IDF_TARGET}'. Remove [extensions.s3_onboard_rgb] for this board.")
    endif()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_S3_ONBOARD_RGB_ENABLED)
endif()
