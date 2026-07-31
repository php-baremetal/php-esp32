# The source list started from the objects that link into a host embed build,
# minus sapi/cli, ext/date, the x86 Zend/asm, and main/internal_functions*
# (replaced by our own internal_functions.c). Trimmed down from there by
# resolving unresolved symbols one at a time.

idf_component_register(
    SRCS
    php-8.3.32/ext/hash/hash_adler32.c
    php-8.3.32/ext/hash/hash_crc32.c
    php-8.3.32/ext/hash/hash_fnv.c
    php-8.3.32/ext/hash/hash_gost.c
    php-8.3.32/ext/hash/hash_haval.c
    php-8.3.32/ext/hash/hash_joaat.c
    php-8.3.32/ext/hash/hash_md.c
    php-8.3.32/ext/hash/hash_murmur.c
    php-8.3.32/ext/hash/hash.c
    php-8.3.32/ext/hash/hash_ripemd.c
    php-8.3.32/ext/hash/hash_sha3.c
    php-8.3.32/ext/hash/hash_sha.c
    php-8.3.32/ext/hash/hash_snefru.c
    php-8.3.32/ext/hash/hash_tiger.c
    php-8.3.32/ext/hash/hash_whirlpool.c
    php-8.3.32/ext/hash/hash_xxhash.c
    php-8.3.32/ext/hash/murmur/PMurHash128.c
    php-8.3.32/ext/hash/murmur/PMurHash.c
    php-8.3.32/ext/hash/sha3/generic64lc/KeccakHash.c
    php-8.3.32/ext/hash/sha3/generic64lc/KeccakP-1600-opt64.c
    php-8.3.32/ext/hash/sha3/generic64lc/KeccakSponge.c
    php-8.3.32/ext/json/json_encoder.c
    php-8.3.32/ext/json/json.c
    php-8.3.32/ext/json/json_parser.tab.c
    php-8.3.32/ext/json/json_scanner.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_auto_possess.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_chartables.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_compile.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_config.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_context.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_convert.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_dfa_match.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_error.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_extuni.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_find_bracket.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_jit_compile.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_maketables.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_match_data.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_match.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_newline.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_ord2utf.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_pattern_info.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_script_run.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_serialize.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_string_utils.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_study.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_substitute.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_substring.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_tables.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_ucd.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_valid_utf.c
    php-8.3.32/ext/pcre/pcre2lib/pcre2_xclass.c
    php-8.3.32/ext/pcre/php_pcre.c
    php-8.3.32/ext/random/csprng.c
    php-8.3.32/ext/random/engine_combinedlcg.c
    php-8.3.32/ext/random/engine_mt19937.c
    php-8.3.32/ext/random/engine_pcgoneseq128xslrr64.c
    php-8.3.32/ext/random/engine_secure.c
    php-8.3.32/ext/random/engine_user.c
    php-8.3.32/ext/random/engine_xoshiro256starstar.c
    php-8.3.32/ext/random/gammasection.c
    php-8.3.32/ext/random/randomizer.c
    php-8.3.32/ext/random/random.c
    php-8.3.32/ext/reflection/php_reflection.c
    php-8.3.32/ext/spl/php_spl.c
    php-8.3.32/ext/spl/spl_array.c
    php-8.3.32/ext/spl/spl_directory.c
    php-8.3.32/ext/spl/spl_dllist.c
    php-8.3.32/ext/spl/spl_exceptions.c
    php-8.3.32/ext/spl/spl_fixedarray.c
    php-8.3.32/ext/spl/spl_functions.c
    php-8.3.32/ext/spl/spl_heap.c
    php-8.3.32/ext/spl/spl_iterators.c
    php-8.3.32/ext/spl/spl_observer.c
    php-8.3.32/ext/standard/array.c
    php-8.3.32/ext/standard/assert.c
    php-8.3.32/ext/standard/base64.c
    php-8.3.32/ext/standard/basic_functions.c
    php-8.3.32/ext/standard/browscap.c
    php-8.3.32/ext/standard/crc32.c
    php-8.3.32/ext/standard/crc32_x86.c
    php-8.3.32/ext/standard/credits.c
    php-8.3.32/ext/standard/crypt_blowfish.c
    php-8.3.32/ext/standard/crypt_freesec.c
    php-8.3.32/ext/standard/crypt.c
    php-8.3.32/ext/standard/crypt_sha256.c
    php-8.3.32/ext/standard/crypt_sha512.c
    php-8.3.32/ext/standard/css.c
    php-8.3.32/ext/standard/datetime.c
    php-8.3.32/ext/standard/dir.c
    php-8.3.32/ext/standard/dl.c
    php-8.3.32/ext/standard/dns.c
    php-8.3.32/ext/standard/exec.c
    php-8.3.32/ext/standard/file.c
    php-8.3.32/ext/standard/filestat.c
    php-8.3.32/ext/standard/filters.c
    php-8.3.32/ext/standard/flock_compat.c
    php-8.3.32/ext/standard/formatted_print.c
    php-8.3.32/ext/standard/fsock.c
    php-8.3.32/ext/standard/ftok.c
    php-8.3.32/ext/standard/ftp_fopen_wrapper.c
    php-8.3.32/ext/standard/head.c
    php-8.3.32/ext/standard/hrtime.c
    php-8.3.32/ext/standard/html.c
    php-8.3.32/ext/standard/http_fopen_wrapper.c
    php-8.3.32/ext/standard/http.c
    php-8.3.32/ext/standard/image.c
    php-8.3.32/ext/standard/incomplete_class.c
    php-8.3.32/ext/standard/info.c
    php-8.3.32/ext/standard/iptc.c
    php-8.3.32/ext/standard/levenshtein.c
    php-8.3.32/ext/standard/libavifinfo/avifinfo.c
    php-8.3.32/ext/standard/link.c
    php-8.3.32/ext/standard/mail.c
    php-8.3.32/ext/standard/math.c
    php-8.3.32/ext/standard/md5.c
    php-8.3.32/ext/standard/metaphone.c
    php-8.3.32/ext/standard/microtime.c
    php-8.3.32/ext/standard/net.c
    php-8.3.32/ext/standard/pack.c
    php-8.3.32/ext/standard/pageinfo.c
    php-8.3.32/ext/standard/password.c
    php-8.3.32/ext/standard/php_crypt_r.c
    php-8.3.32/ext/standard/php_fopen_wrapper.c
    php-8.3.32/ext/standard/proc_open.c
    php-8.3.32/ext/standard/quot_print.c
    php-8.3.32/ext/standard/scanf.c
    php-8.3.32/ext/standard/sha1.c
    php-8.3.32/ext/standard/soundex.c
    php-8.3.32/ext/standard/streamsfuncs.c
    php-8.3.32/ext/standard/string.c
    php-8.3.32/ext/standard/strnatcmp.c
    php-8.3.32/ext/standard/syslog.c
    php-8.3.32/ext/standard/type.c
    php-8.3.32/ext/standard/uniqid.c
    php-8.3.32/ext/standard/url.c
    php-8.3.32/ext/standard/url_scanner_ex.c
    php-8.3.32/ext/standard/user_filters.c
    php-8.3.32/ext/standard/uuencode.c
    php-8.3.32/ext/standard/var.c
    php-8.3.32/ext/standard/var_unserializer.c
    php-8.3.32/ext/standard/versioning.c
    php-8.3.32/main/explicit_bzero.c
    php-8.3.32/main/fopen_wrappers.c
    php-8.3.32/main/getopt.c
    php-8.3.32/main/main.c
    php-8.3.32/main/network.c
    php-8.3.32/main/output.c
    php-8.3.32/main/php_content_types.c
    php-8.3.32/main/php_ini_builder.c
    php-8.3.32/main/php_ini.c
    php-8.3.32/main/php_odbc_utils.c
    php-8.3.32/main/php_open_temporary_file.c
    php-8.3.32/main/php_scandir.c
    php-8.3.32/main/php_syslog.c
    php-8.3.32/main/php_ticks.c
    php-8.3.32/main/php_variables.c
    php-8.3.32/main/reentrancy.c
    php-8.3.32/main/rfc1867.c
    php-8.3.32/main/safe_bcmp.c
    php-8.3.32/main/SAPI.c
    php-8.3.32/main/snprintf.c
    php-8.3.32/main/spprintf.c
    php-8.3.32/main/streams/cast.c
    php-8.3.32/main/streams/filter.c
    php-8.3.32/main/streams/glob_wrapper.c
    php-8.3.32/main/streams/memory.c
    php-8.3.32/main/streams/mmap.c
    php-8.3.32/main/streams/plain_wrapper.c
    php-8.3.32/main/streams/streams.c
    php-8.3.32/main/streams/transports.c
    php-8.3.32/main/streams/userspace.c
    php-8.3.32/main/streams/xp_socket.c
    php-8.3.32/main/strlcat.c
    php-8.3.32/main/strlcpy.c
    php-8.3.32/sapi/embed/php_embed.c
    php-8.3.32/TSRM/TSRM.c
    php-8.3.32/Zend/Optimizer/block_pass.c
    php-8.3.32/Zend/Optimizer/compact_literals.c
    php-8.3.32/Zend/Optimizer/compact_vars.c
    php-8.3.32/Zend/Optimizer/dce.c
    php-8.3.32/Zend/Optimizer/dfa_pass.c
    php-8.3.32/Zend/Optimizer/escape_analysis.c
    php-8.3.32/Zend/Optimizer/nop_removal.c
    php-8.3.32/Zend/Optimizer/optimize_func_calls.c
    php-8.3.32/Zend/Optimizer/optimize_temp_vars_5.c
    php-8.3.32/Zend/Optimizer/pass1.c
    php-8.3.32/Zend/Optimizer/pass3.c
    php-8.3.32/Zend/Optimizer/sccp.c
    php-8.3.32/Zend/Optimizer/scdf.c
    php-8.3.32/Zend/Optimizer/zend_call_graph.c
    php-8.3.32/Zend/Optimizer/zend_cfg.c
    php-8.3.32/Zend/Optimizer/zend_dfg.c
    php-8.3.32/Zend/Optimizer/zend_dump.c
    php-8.3.32/Zend/Optimizer/zend_func_info.c
    php-8.3.32/Zend/Optimizer/zend_inference.c
    php-8.3.32/Zend/Optimizer/zend_optimizer.c
    php-8.3.32/Zend/Optimizer/zend_ssa.c
    php-8.3.32/Zend/zend_alloc.c
    php-8.3.32/Zend/zend_API.c
    php-8.3.32/Zend/zend_ast.c
    php-8.3.32/Zend/zend_atomic.c
    php-8.3.32/Zend/zend_attributes.c
    php-8.3.32/Zend/zend_builtin_functions.c
    php-8.3.32/Zend/zend_call_stack.c
    php-8.3.32/Zend/zend_closures.c
    php-8.3.32/Zend/zend_compile.c
    php-8.3.32/Zend/zend_constants.c
    php-8.3.32/Zend/zend_cpuinfo.c
    php-8.3.32/Zend/zend_default_classes.c
    php-8.3.32/Zend/zend_dtrace.c
    php-8.3.32/Zend/zend_enum.c
    php-8.3.32/Zend/zend_exceptions.c
    php-8.3.32/Zend/zend_execute_API.c
    php-8.3.32/Zend/zend_execute.c
    php-8.3.32/Zend/zend_extensions.c
    php-8.3.32/Zend/zend_fibers.c
    php-8.3.32/Zend/zend_float.c
    php-8.3.32/Zend/zend_gc.c
    php-8.3.32/Zend/zend_gdb.c
    php-8.3.32/Zend/zend_generators.c
    php-8.3.32/Zend/zend_hash.c
    php-8.3.32/Zend/zend_highlight.c
    php-8.3.32/Zend/zend_hrtime.c
    php-8.3.32/Zend/zend_inheritance.c
    php-8.3.32/Zend/zend_ini.c
    php-8.3.32/Zend/zend_ini_parser.c
    php-8.3.32/Zend/zend_ini_scanner.c
    php-8.3.32/Zend/zend_interfaces.c
    php-8.3.32/Zend/zend_iterators.c
    php-8.3.32/Zend/zend_language_parser.c
    php-8.3.32/Zend/zend_language_scanner.c
    php-8.3.32/Zend/zend_list.c
    php-8.3.32/Zend/zend_llist.c
    php-8.3.32/Zend/zend_max_execution_timer.c
    php-8.3.32/Zend/zend_multibyte.c
    php-8.3.32/Zend/zend.c
    php-8.3.32/Zend/zend_object_handlers.c
    php-8.3.32/Zend/zend_objects_API.c
    php-8.3.32/Zend/zend_objects.c
    php-8.3.32/Zend/zend_observer.c
    php-8.3.32/Zend/zend_opcode.c
    php-8.3.32/Zend/zend_operators.c
    php-8.3.32/Zend/zend_ptr_stack.c
    php-8.3.32/Zend/zend_signal.c
    php-8.3.32/Zend/zend_smart_str.c
    php-8.3.32/Zend/zend_sort.c
    php-8.3.32/Zend/zend_stack.c
    php-8.3.32/Zend/zend_stream.c
    php-8.3.32/Zend/zend_string.c
    php-8.3.32/Zend/zend_strtod.c
    php-8.3.32/Zend/zend_system_id.c
    php-8.3.32/Zend/zend_variables.c
    php-8.3.32/Zend/zend_virtual_cwd.c
    php-8.3.32/Zend/zend_vm_opcodes.c
    php-8.3.32/Zend/zend_weakrefs.c
    # --- our own sources (not from the tarball) ---
    "internal_functions.c"
    "compat/syslog.c"
    "compat/posix_stubs.c"
    # compat/date_stub.c OR the real ext/date is added below, depending on
    # PHP_EXT_DATE (see the optional-extensions block).

    # Public: what `main` needs to include the engine (php_config.h lives in ".").
    INCLUDE_DIRS
        "."
        "compat"
        "php-8.3.32"
        "php-8.3.32/main"
        "php-8.3.32/Zend"
        "php-8.3.32/TSRM"
        "php-8.3.32/sapi/embed"
    # Private: the per-file include dirs a host build passes; date/cli/asm dropped.
    PRIV_INCLUDE_DIRS
        "php-8.3.32/main/streams"
        "php-8.3.32/ext/hash"
        "php-8.3.32/ext/hash/sha3/generic64lc"
        "php-8.3.32/ext/hash/xxhash"
        "php-8.3.32/ext/json"
        "php-8.3.32/ext/pcre"
        "php-8.3.32/ext/pcre/pcre2lib"
        "php-8.3.32/ext/random"
        "php-8.3.32/ext/reflection"
        "php-8.3.32/ext/spl"
        "php-8.3.32/ext/standard"
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
        php-8.3.32/ext/date/php_date.c
        php-8.3.32/ext/date/lib/astro.c
        php-8.3.32/ext/date/lib/dow.c
        php-8.3.32/ext/date/lib/interval.c
        php-8.3.32/ext/date/lib/parse_date.c
        php-8.3.32/ext/date/lib/parse_iso_intervals.c
        php-8.3.32/ext/date/lib/parse_posix.c
        php-8.3.32/ext/date/lib/parse_tz.c
        php-8.3.32/ext/date/lib/timelib.c
        php-8.3.32/ext/date/lib/tm2unixtime.c
        php-8.3.32/ext/date/lib/unixtime2tm.c
    )
    target_include_directories(${COMPONENT_LIB} PRIVATE
        php-8.3.32/ext/date
        php-8.3.32/ext/date/lib
    )
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_DATE_ENABLED)
    if(PHP_EXT_DATE_MINIMAL_TZ)
        # parse_tz.c (patched) picks up compat/timezonedb_minimal.h, already on the path.
        target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_DATE_MINIMAL_TZ)
    endif()
else()
    target_sources(${COMPONENT_LIB} PRIVATE compat/date_stub.c)
endif()

# PDO + SQLite: read/write .db files on the microSD. Enable with:
#   idf.py -DPHP_EXT_SQLITE=ON ...   (or answer "y" in flash.sh)
# Needs the SQLite amalgamation: ./scripts/fetch-sqlite.sh
option(PHP_EXT_SQLITE "Build the PDO/SQLite extension" OFF)
if(PHP_EXT_SQLITE)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/sqlite-amalgamation/sqlite3.c")
        message(FATAL_ERROR
            "PHP_EXT_SQLITE=ON but the SQLite amalgamation is missing. "
            "Run ./scripts/fetch-sqlite.sh first.")
    endif()
    target_sources(${COMPONENT_LIB} PRIVATE
        php-8.3.32/ext/pdo/pdo.c
        php-8.3.32/ext/pdo/pdo_dbh.c
        php-8.3.32/ext/pdo/pdo_sql_parser.c
        php-8.3.32/ext/pdo/pdo_sqlstate.c
        php-8.3.32/ext/pdo/pdo_stmt.c
        php-8.3.32/ext/pdo_sqlite/pdo_sqlite.c
        php-8.3.32/ext/pdo_sqlite/sqlite_driver.c
        php-8.3.32/ext/pdo_sqlite/sqlite_statement.c
        sqlite-amalgamation/sqlite3.c
    )
    target_include_directories(${COMPONENT_LIB} PRIVATE
        sqlite-amalgamation
        php-8.3.32/ext
    )
    # Force-include a small shim into sqlite3.c only: it neutralizes the project's
    # global HAVE_*INT*_T macros (which would misconfigure SQLite's integer
    # typedefs) and maps lstat->stat. Scoped to this file so nothing else changes.
    set_source_files_properties(sqlite-amalgamation/sqlite3.c PROPERTIES
        COMPILE_OPTIONS "-include;${CMAKE_CURRENT_LIST_DIR}/compat/sqlite-compat.h")
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
    target_sources(${COMPONENT_LIB} PRIVATE php-8.3.32/ext/ctype/ctype.c)
    target_include_directories(${COMPONENT_LIB} PRIVATE php-8.3.32/ext/ctype)
    # ctype.c wraps its whole body (module entry included) in #ifdef HAVE_CTYPE,
    # which configure would define. Scope it to this one file so nothing else sees it.
    set_source_files_properties(php-8.3.32/ext/ctype/ctype.c PROPERTIES
        COMPILE_DEFINITIONS "HAVE_CTYPE")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_CTYPE_ENABLED)
endif()

# ext/filter: filter_var() validation and sanitization. Depends on ext/pcre (built
# in) and ext/standard.
# Enable with idf.py -DPHP_EXT_FILTER=ON (or "y" in flash.sh).
option(PHP_EXT_FILTER "Build the ext/filter extension" OFF)
if(PHP_EXT_FILTER)
    target_sources(${COMPONENT_LIB} PRIVATE
        php-8.3.32/ext/filter/filter.c
        php-8.3.32/ext/filter/logical_filters.c
        php-8.3.32/ext/filter/sanitizing_filters.c
        php-8.3.32/ext/filter/callback_filter.c
    )
    target_include_directories(${COMPONENT_LIB} PRIVATE php-8.3.32/ext/filter)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_FILTER_ENABLED)
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
        php-8.3.32/ext/mbstring/mbstring.c
        php-8.3.32/ext/mbstring/php_unicode.c
        php-8.3.32/ext/mbstring/mb_gpc.c
        # bundled libmbfl (charset conversion) -- the whole library, minus nothing.
        php-8.3.32/ext/mbstring/libmbfl/filters/html_entities.c
        php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_7bit.c
        php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_base64.c
        php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_htmlent.c
        php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_qprint.c
        php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_singlebyte.c
        php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_ucs2.c
        php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_ucs4.c
        php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_utf16.c
        php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_utf32.c
        php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_utf7.c
        php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_utf7imap.c
        php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_utf8.c
        php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_uuencode.c
        php-8.3.32/ext/mbstring/libmbfl/mbfl/mbfilter_8bit.c
        php-8.3.32/ext/mbstring/libmbfl/mbfl/mbfilter.c
        php-8.3.32/ext/mbstring/libmbfl/mbfl/mbfilter_pass.c
        php-8.3.32/ext/mbstring/libmbfl/mbfl/mbfilter_wchar.c
        php-8.3.32/ext/mbstring/libmbfl/mbfl/mbfl_convert.c
        php-8.3.32/ext/mbstring/libmbfl/mbfl/mbfl_encoding.c
        php-8.3.32/ext/mbstring/libmbfl/mbfl/mbfl_filter_output.c
        php-8.3.32/ext/mbstring/libmbfl/mbfl/mbfl_language.c
        php-8.3.32/ext/mbstring/libmbfl/mbfl/mbfl_memory_device.c
        php-8.3.32/ext/mbstring/libmbfl/mbfl/mbfl_string.c
        php-8.3.32/ext/mbstring/libmbfl/nls/nls_de.c
        php-8.3.32/ext/mbstring/libmbfl/nls/nls_en.c
        php-8.3.32/ext/mbstring/libmbfl/nls/nls_hy.c
        php-8.3.32/ext/mbstring/libmbfl/nls/nls_ja.c
        php-8.3.32/ext/mbstring/libmbfl/nls/nls_kr.c
        php-8.3.32/ext/mbstring/libmbfl/nls/nls_neutral.c
        php-8.3.32/ext/mbstring/libmbfl/nls/nls_ru.c
        php-8.3.32/ext/mbstring/libmbfl/nls/nls_tr.c
        php-8.3.32/ext/mbstring/libmbfl/nls/nls_ua.c
        php-8.3.32/ext/mbstring/libmbfl/nls/nls_uni.c
        php-8.3.32/ext/mbstring/libmbfl/nls/nls_zh.c
    )
    # The legacy CJK codecs live in mbfilter_cjk.c (+ mbfilter_utf8_mobile.c, which reuses
    # its emoji tables) -- ~740 KB. Make them optional: with PHP_EXT_MBSTRING_NO_CJK they're
    # dropped and the 0003 patch removes their registry entries (guarded by MBSTRING_NO_CJK)
    # so nothing references the missing symbols.
    if(NOT PHP_EXT_MBSTRING_NO_CJK)
        target_sources(${COMPONENT_LIB} PRIVATE
            php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_cjk.c
            php-8.3.32/ext/mbstring/libmbfl/filters/mbfilter_utf8_mobile.c
        )
    else()
        target_compile_definitions(${COMPONENT_LIB} PRIVATE MBSTRING_NO_CJK)
    endif()
    target_include_directories(${COMPONENT_LIB} PRIVATE
        ${_mbfl_cfg_dir}
        php-8.3.32/ext/mbstring
        php-8.3.32/ext/mbstring/libmbfl
        php-8.3.32/ext/mbstring/libmbfl/mbfl
    )
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_EXT_MBSTRING_ENABLED)

    # mb_ereg*/mb_split (multibyte regex). PHP doesn't bundle its regex engine, so this
    # compiles the vendored oniguruma (git-ignored: ./scripts/fetch-oniguruma.sh) plus
    # php_mbregex.c, and defines HAVE_MBREGEX so mbstring.c and its arginfo register the
    # mb_ereg* / mb_split functions. Without this the family simply isn't there.
    if(PHP_EXT_MBSTRING_ONIG)
        if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/oniguruma/src/oniguruma.h")
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
            php-8.3.32/ext/mbstring/php_mbregex.c
            ${_onig}
        )
        # HAVE_MBREGEX gates the mb_ereg* code in mbstring.c and its arginfo entries.
        target_compile_definitions(${COMPONENT_LIB} PRIVATE HAVE_MBREGEX)
        # php_mbregex.c pulls <oniguruma.h> and needs ONIG_ESCAPE_UCHAR_COLLISION so
        # oniguruma.h doesn't re-typedef UChar (php.h already has it). Scoped to that file
        # so the include of oniguruma/src doesn't leak a bare "config.h" onto other units.
        set_source_files_properties(php-8.3.32/ext/mbstring/php_mbregex.c PROPERTIES
            INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/oniguruma/src"
            COMPILE_DEFINITIONS "ONIG_ESCAPE_UCHAR_COLLISION=1")
    endif()
endif()
