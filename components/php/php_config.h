/* main/php_config.h.  Generated from php_config.h.in by configure.  */
/* main/php_config.h.in.  Generated from configure.ac by autoheader.  */


#ifndef PHP_CONFIG_H
#define PHP_CONFIG_H

#if defined(__GNUC__) && __GNUC__ >= 4
# define ZEND_API __attribute__ ((visibility("default")))
# define ZEND_DLEXPORT __attribute__ ((visibility("default")))
#else
# define ZEND_API
# define ZEND_DLEXPORT
#endif

#define ZEND_DLIMPORT


/* Uses cmsgcred struct */
/* #undef ANC_CREDS_CMSGCRED */

/* Uses ucred struct */
/* #undef ANC_CREDS_UCRED */

/* */
/* #undef CDB_INCLUDE_FILE */

/* Whether to build bcmath as dynamic module */
/* #undef COMPILE_DL_BCMATH */

/* Whether to build bz2 as dynamic module */
/* #undef COMPILE_DL_BZ2 */

/* Whether to build calendar as dynamic module */
/* #undef COMPILE_DL_CALENDAR */

/* Whether to build ctype as dynamic module */
/* #undef COMPILE_DL_CTYPE */

/* Whether to build curl as dynamic module */
/* #undef COMPILE_DL_CURL */

/* Whether to build date as dynamic module */
/* #undef COMPILE_DL_DATE */

/* Whether to build dba as dynamic module */
/* #undef COMPILE_DL_DBA */

/* Whether to build dl_test as dynamic module */
/* #undef COMPILE_DL_DL_TEST */

/* Whether to build dom as dynamic module */
/* #undef COMPILE_DL_DOM */

/* Whether to build enchant as dynamic module */
/* #undef COMPILE_DL_ENCHANT */

/* Whether to build exif as dynamic module */
/* #undef COMPILE_DL_EXIF */

/* Whether to build ffi as dynamic module */
/* #undef COMPILE_DL_FFI */

/* Whether to build fileinfo as dynamic module */
/* #undef COMPILE_DL_FILEINFO */

/* Whether to build filter as dynamic module */
/* #undef COMPILE_DL_FILTER */

/* Whether to build ftp as dynamic module */
/* #undef COMPILE_DL_FTP */

/* Whether to build gd as dynamic module */
/* #undef COMPILE_DL_GD */

/* Whether to build gettext as dynamic module */
/* #undef COMPILE_DL_GETTEXT */

/* Whether to build gmp as dynamic module */
/* #undef COMPILE_DL_GMP */

/* Whether to build hash as dynamic module */
/* #undef COMPILE_DL_HASH */

/* Whether to build iconv as dynamic module */
/* #undef COMPILE_DL_ICONV */

/* Whether to build imap as dynamic module */
/* #undef COMPILE_DL_IMAP */

/* Whether to build intl as dynamic module */
/* #undef COMPILE_DL_INTL */

/* Whether to build json as dynamic module */
/* #undef COMPILE_DL_JSON */

/* Whether to build ldap as dynamic module */
/* #undef COMPILE_DL_LDAP */

/* Whether to build libxml as dynamic module */
/* #undef COMPILE_DL_LIBXML */

/* Whether to build mbstring as dynamic module */
/* #undef COMPILE_DL_MBSTRING */

/* Whether to build mysqli as dynamic module */
/* #undef COMPILE_DL_MYSQLI */

/* Whether to build mysqlnd as dynamic module */
/* #undef COMPILE_DL_MYSQLND */

/* Whether to build oci8 as dynamic module */
/* #undef COMPILE_DL_OCI8 */

/* Whether to build odbc as dynamic module */
/* #undef COMPILE_DL_ODBC */

/* Whether to build opcache as dynamic module */
/* #undef COMPILE_DL_OPCACHE */

/* Whether to build openssl as dynamic module */
/* #undef COMPILE_DL_OPENSSL */

/* Whether to build pcntl as dynamic module */
/* #undef COMPILE_DL_PCNTL */

/* Whether to build pcre as dynamic module */
/* #undef COMPILE_DL_PCRE */

/* Whether to build pdo as dynamic module */
/* #undef COMPILE_DL_PDO */

/* Whether to build pdo_dblib as dynamic module */
/* #undef COMPILE_DL_PDO_DBLIB */

/* Whether to build pdo_firebird as dynamic module */
/* #undef COMPILE_DL_PDO_FIREBIRD */

/* Whether to build pdo_mysql as dynamic module */
/* #undef COMPILE_DL_PDO_MYSQL */

/* Whether to build pdo_oci as dynamic module */
/* #undef COMPILE_DL_PDO_OCI */

/* Whether to build pdo_odbc as dynamic module */
/* #undef COMPILE_DL_PDO_ODBC */

/* Whether to build pdo_pgsql as dynamic module */
/* #undef COMPILE_DL_PDO_PGSQL */

/* Whether to build pdo_sqlite as dynamic module */
/* #undef COMPILE_DL_PDO_SQLITE */

/* Whether to build pgsql as dynamic module */
/* #undef COMPILE_DL_PGSQL */

/* Whether to build phar as dynamic module */
/* #undef COMPILE_DL_PHAR */

/* Whether to build posix as dynamic module */
/* #undef COMPILE_DL_POSIX */

/* Whether to build pspell as dynamic module */
/* #undef COMPILE_DL_PSPELL */

/* Whether to build random as dynamic module */
/* #undef COMPILE_DL_RANDOM */

/* Whether to build readline as dynamic module */
/* #undef COMPILE_DL_READLINE */

/* Whether to build reflection as dynamic module */
/* #undef COMPILE_DL_REFLECTION */

/* Whether to build session as dynamic module */
/* #undef COMPILE_DL_SESSION */

/* Whether to build shmop as dynamic module */
/* #undef COMPILE_DL_SHMOP */

/* Whether to build simplexml as dynamic module */
/* #undef COMPILE_DL_SIMPLEXML */

/* Whether to build snmp as dynamic module */
/* #undef COMPILE_DL_SNMP */

/* Whether to build soap as dynamic module */
/* #undef COMPILE_DL_SOAP */

/* Whether to build sockets as dynamic module */
/* #undef COMPILE_DL_SOCKETS */

/* Whether to build sodium as dynamic module */
/* #undef COMPILE_DL_SODIUM */

/* Whether to build spl as dynamic module */
/* #undef COMPILE_DL_SPL */

/* Whether to build sqlite3 as dynamic module */
/* #undef COMPILE_DL_SQLITE3 */

/* Whether to build standard as dynamic module */
/* #undef COMPILE_DL_STANDARD */

/* Whether to build sysvmsg as dynamic module */
/* #undef COMPILE_DL_SYSVMSG */

/* Whether to build sysvsem as dynamic module */
/* #undef COMPILE_DL_SYSVSEM */

/* Whether to build sysvshm as dynamic module */
/* #undef COMPILE_DL_SYSVSHM */

/* Whether to build tidy as dynamic module */
/* #undef COMPILE_DL_TIDY */

/* Whether to build tokenizer as dynamic module */
/* #undef COMPILE_DL_TOKENIZER */

/* Whether to build xml as dynamic module */
/* #undef COMPILE_DL_XML */

/* Whether to build xmlreader as dynamic module */
/* #undef COMPILE_DL_XMLREADER */

/* Whether to build xmlwriter as dynamic module */
/* #undef COMPILE_DL_XMLWRITER */

/* Whether to build xsl as dynamic module */
/* #undef COMPILE_DL_XSL */

/* Whether to build zend_test as dynamic module */
/* #undef COMPILE_DL_ZEND_TEST */

/* Whether to build zip as dynamic module */
/* #undef COMPILE_DL_ZIP */

/* Whether to build zlib as dynamic module */
/* #undef COMPILE_DL_ZLIB */

/* */
#define COOKIE_IO_FUNCTIONS_T cookie_io_functions_t

/* */
#define COOKIE_SEEKER_USES_OFF64_T 1

/* Define if crypt_r has uses CRYPTD */
/* #undef CRYPT_R_CRYPTD */

/* Define if struct crypt_data requires _GNU_SOURCE */
/* #undef CRYPT_R_GNU_SOURCE */

/* Define if crypt_r uses struct crypt_data */
/* #undef CRYPT_R_STRUCT_CRYPT_DATA */

/* Define to 1 if using 'alloca.c'. */
/* #undef C_ALLOCA */

/* Define if the target system is darwin */
/* #undef DARWIN */

/* */
/* #undef DB1_INCLUDE_FILE */

/* */
/* #undef DB1_VERSION */

/* */
/* #undef DB2_INCLUDE_FILE */

/* */
/* #undef DB3_INCLUDE_FILE */

/* */
/* #undef DB4_INCLUDE_FILE */

/* */
/* #undef DBA_CDB */

/* */
/* #undef DBA_CDB_BUILTIN */

/* */
/* #undef DBA_CDB_MAKE */

/* */
/* #undef DBA_DB1 */

/* */
/* #undef DBA_DB2 */

/* */
/* #undef DBA_DB3 */

/* */
/* #undef DBA_DB4 */

/* */
/* #undef DBA_DBM */

/* */
/* #undef DBA_FLATFILE */

/* */
/* #undef DBA_GDBM */

/* */
/* #undef DBA_INIFILE */

/* */
/* #undef DBA_LMDB */

/* */
/* #undef DBA_NDBM */

/* */
/* #undef DBA_QDBM */

/* */
/* #undef DBA_TCADB */

/* */
/* #undef DBM_INCLUDE_FILE */

/* */
/* #undef DBM_VERSION */

/* */
#define DEFAULT_SHORT_OPEN_TAG "1"

/* Define if dlsym() requires a leading underscore in symbol names. */
/* #undef DLSYM_NEEDS_UNDERSCORE */

/* Whether to enable chroot() function */
#define ENABLE_CHROOT_FUNC 1

/* */
/* #undef GDBM_INCLUDE_FILE */

/* Define to 1 if 'TIOCGWINSZ' requires <sys/ioctl.h>. */
/* #undef GWINSZ_IN_SYS_IOCTL */

/* Whether 3 arg set_rebind_proc() */
/* #undef HAVE_3ARG_SETREBINDPROC */

/* Define to 1 when aarch64 CRC32 API is available. */
/* #undef HAVE_AARCH64_CRC32 */

/* */
/* #undef HAVE_ADABAS */

/* Whether you have AI_ALL */
/* #undef HAVE_AI_ALL */

/* Whether you have AI_IDN */
/* #undef HAVE_AI_IDN */

/* Whether you have AI_V4MAPPED */
/* #undef HAVE_AI_V4MAPPED */

/* whether the compiler supports __alignof__ */
#define HAVE_ALIGNOF 1

/* Define to 1 if you have 'alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if <alloca.h> works. */
#define HAVE_ALLOCA_H 1

/* Define to 1 if you have the 'alphasort' function. */
#define HAVE_ALPHASORT 1

/* AppArmor confinement available */
/* #undef HAVE_APPARMOR */

/* */
/* #undef HAVE_ARGON2LIB */

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define to 1 if you have the <arpa/nameser.h> header file. */
/* #undef HAVE_ARPA_NAMESER_H -- not available on this target */

/* Define to 1 if you have the 'asctime_r' function. */
#define HAVE_ASCTIME_R 1

/* Define if asm goto support */
#define HAVE_ASM_GOTO 1

/* Define to 1 if you have the 'asprintf' function. */
#define HAVE_ASPRINTF 1

/* Define to 1 if you have the 'atoll' function. */
#define HAVE_ATOLL 1

/* whether the compiler supports __attribute__ ((__aligned__)) */
#define HAVE_ATTRIBUTE_ALIGNED 1

/* Whether you have bcmath */
/* #undef HAVE_BCMATH */

/* */
/* #undef HAVE_BIND_TEXTDOMAIN_CODESET */

/* Define if system has broken getcwd */
/* #undef HAVE_BROKEN_GETCWD */

/* Konstantin Chuguev's iconv implementation */
/* #undef HAVE_BSD_ICONV */

/* */
#define HAVE_BUILD_DEFS_H 1

/* Define to 1 if gcc supports __sync_bool_compare_and_swap() a.o. */
/* #undef HAVE_BUILTIN_ATOMIC */

/* */
#define HAVE_BUNDLED_PCRE 1

/* */
/* #undef HAVE_BZ2 */

/* */
/* #undef HAVE_CALENDAR */

/* Libzip >= 1.6.0 with zip_register_cancel_callback_with_state function */
/* #undef HAVE_CANCEL_CALLBACK */

/* Capstone is available */
/* #undef HAVE_CAPSTONE */

/* Define to 1 if you have the 'chroot' function. */
#define HAVE_CHROOT 1

/* Define to 1 if you have the 'clearenv' function. */
/* #undef HAVE_CLEARENV */

/* */
/* #undef HAVE_CLI0CLI_H */

/* */
/* #undef HAVE_CLI0CORE_H */

/* */
/* #undef HAVE_CLI0DEFS_H */

/* */
/* #undef HAVE_CLI0ENV_H */

/* */
/* #undef HAVE_CLI0EXT_H */

/* do we have clock_gettime? */
/* #undef HAVE_CLOCK_GETTIME */

/* do we have clock_get_time? */
/* #undef HAVE_CLOCK_GET_TIME */

/* */
/* #undef HAVE_CODBC */

/* Define to 1 if you have the <CommonCrypto/CommonRandom.h> header file. */
/* #undef HAVE_COMMONCRYPTO_COMMONRANDOM_H */

/* Define if copy_file_range support */
#define HAVE_COPY_FILE_RANGE 1

/* whether __cpuid_count is available */
#define HAVE_CPUID_COUNT 1

/* Define to 1 if you have the <cpuid.h> header file. */
#define HAVE_CPUID_H 1

/* Define to 1 if you have the 'CreateProcess' function. */
/* #undef HAVE_CREATEPROCESS */

/* */
/* #undef HAVE_CRYPT */

/* Define to 1 if you have the <crypt.h> header file. */
#define HAVE_CRYPT_H 1

/* */
/* #undef HAVE_CRYPT_R */

/* Define to 1 if you have the 'ctermid' function. */
/* #undef HAVE_CTERMID */

/* Define to 1 if you have the 'ctime_r' function. */
#define HAVE_CTIME_R 1

/* */
/* #undef HAVE_CTYPE */

/* */
/* #undef HAVE_CURL */

/* Have cURL with old OpenSSL */
/* #undef HAVE_CURL_OLD_OPENSSL */

/* */
/* #undef HAVE_DBA */

/* Whether you want DBMaker */
/* #undef HAVE_DBMAKER */

/* */
/* #undef HAVE_DCNGETTEXT */

/* Define to 1 if you have the declaration of 'arc4random_buf', and to 0 if
   you don't. */
#define HAVE_DECL_ARC4RANDOM_BUF 1

/* Define to 1 if you have the declaration of 'strerror_r', and to 0 if you
   don't. */
#define HAVE_DECL_STRERROR_R 1

/* Define to 1 if you have the declaration of 'tzname', and to 0 if you don't.
   */
/* #undef HAVE_DECL_TZNAME */

/* do we have /dev/poll? */
/* #undef HAVE_DEVPOLL */

/* Define to 1 if you have the <dirent.h> header file. */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H -- no dynamic loading; everything is linked statically */

/* */
/* #undef HAVE_DLOPEN -- no dynamic loading; everything is linked statically */

/* */
/* #undef HAVE_DLSYM -- no dynamic loading; everything is linked statically */

/* Whether you have dmalloc */
/* #undef HAVE_DMALLOC */

/* */
/* #undef HAVE_DNGETTEXT */

/* Define to 1 if you have the <dns.h> header file. */
/* #undef HAVE_DNS_H */

/* */
/* #undef HAVE_DNS_SEARCH */

/* */
/* #undef HAVE_DN_EXPAND -- no DNS resolver; this board has no networking */

/* */
/* #undef HAVE_DN_SKIPNAME -- no DNS resolver; this board has no networking */

/* */
/* #undef HAVE_DOM */

/* Whether to enable DTrace support */
/* #undef HAVE_DTRACE */

/* Define to 1 if you have the 'eaccess' function. */
/* #undef HAVE_EACCESS */

/* */
/* #undef HAVE_EMPRESS */

/* */
/* #undef HAVE_ENCHANT */

/* enchant_broker_set_param since 1.5.0 and removed in 2.x */
/* #undef HAVE_ENCHANT_BROKER_SET_PARAM */

/* enchant_get_version since 1.6.0 */
/* #undef HAVE_ENCHANT_GET_VERSION */

/* Libzip >= 1.2.0 with encryption support */
/* #undef HAVE_ENCRYPTION */

/* do we have epoll? */
/* #undef HAVE_EPOLL */

/* */
/* #undef HAVE_ESOOB */

/* Whether you want EXIF (metadata from images) support */
/* #undef HAVE_EXIF */

/* Define to 1 if you have the 'explicit_bzero' function. */
#define HAVE_EXPLICIT_BZERO 1

/* Define to 1 if you have the 'explicit_memset' function. */
/* #undef HAVE_EXPLICIT_MEMSET */

/* Define to 1 if you have the 'fabsf' function. */
/* #undef HAVE_FABSF */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the 'fdatasync' function. */
#define HAVE_FDATASYNC 1

/* Have ffi support */
/* #undef HAVE_FFI */

/* Whether libffi supports fastcall calling convention */
/* #undef HAVE_FFI_FASTCALL */

/* Whether libffi supports ms_cdecl calling convention */
/* #undef HAVE_FFI_MS_CDECL */

/* Whether libffi supports pascal calling convention */
/* #undef HAVE_FFI_PASCAL */

/* Whether libffi supports register calling convention */
/* #undef HAVE_FFI_REGISTER */

/* Whether libffi supports stdcall calling convention */
/* #undef HAVE_FFI_STDCALL */

/* Whether libffi supports sysv calling convention */
/* #undef HAVE_FFI_SYSV */

/* Whether libffi supports thiscall calling convention */
/* #undef HAVE_FFI_THISCALL */

/* Define to 1 if you have the 'flock' function. */
#define HAVE_FLOCK 1

/* Define to 1 if you have the 'floorf' function. */
/* #undef HAVE_FLOORF */

/* Define if flush should be called explicitly after a buffered io. */
/* #undef HAVE_FLUSHIO */

/* Define to 1 if your system has a working POSIX 'fnmatch' function. */
#define HAVE_FNMATCH 1

/* */
/* #undef HAVE_FOPENCOOKIE -- newlib's cookie signature uses off_t, not glibc's off64_t; use the fallback */

/* Define to 1 if you have the 'fork' function. */
#define HAVE_FORK 1

/* Define to 1 if you have the 'forkx' function. */
/* #undef HAVE_FORKX */

/* Define to 1 if you have the 'fpathconf' function. */
/* #undef HAVE_FPATHCONF */

/* do we have acl support? */
/* #undef HAVE_FPM_ACL */

/* whether fpsetprec is present and usable */
/* #undef HAVE_FPSETPREC */

/* whether FPU control word can be manipulated by inline assembler */
/* #undef HAVE_FPU_INLINE_ASM_X86 -- the x87 fnstcw/fldcw asm doesn't exist on RISC-V */

/* Define to 1 if you have the 'ftok' function. */
/* #undef HAVE_FTOK -- not available on this target */

/* Whether you want FTP support */
/* #undef HAVE_FTP */

/* Whether FTP over SSL is supported */
/* #undef HAVE_FTP_SSL */

/* Define to 1 if the system has the `ifunc' function attribute */
#define HAVE_FUNC_ATTRIBUTE_IFUNC 1

/* Define to 1 if the system has the `target' function attribute */
#define HAVE_FUNC_ATTRIBUTE_TARGET 1

/* Define to 1 if you have the three-argument form of gethostbyname_r(). */
/* #undef HAVE_FUNC_GETHOSTBYNAME_R_3 */

/* Define to 1 if you have the five-argument form of gethostbyname_r(). */
/* #undef HAVE_FUNC_GETHOSTBYNAME_R_5 */

/* Define to 1 if you have the six-argument form of gethostbyname_r(). */
#define HAVE_FUNC_GETHOSTBYNAME_R_6 1

/* Define to 1 if you have the 'funopen' function. */
/* #undef HAVE_FUNOPEN */

/* Define to 1 if you have the 'gai_strerror' function. */
#define HAVE_GAI_STRERROR 1

/* Define if the target system has support for global register variables */
/* #undef HAVE_GCC_GLOBAL_REGS -- hybrid register-global VM isn't reliable on RISC-V; use the portable call VM */

/* Whether you have gcov */
/* #undef HAVE_GCOV */

/* Does gdImageCreateFromAvif work? */
/* #undef HAVE_GD_AVIF */

/* Does gdImageCreateFromBmp work? */
/* #undef HAVE_GD_BMP */

/* */
/* #undef HAVE_GD_BUNDLED */

/* */
/* #undef HAVE_GD_FREETYPE */

/* */
/* #undef HAVE_GD_GET_INTERPOLATION */

/* Does gdImageCreateFromJpeg work? */
/* #undef HAVE_GD_JPG */

/* */
/* #undef HAVE_GD_LIBVERSION */

/* Does gdImageCreateFromPng work? */
/* #undef HAVE_GD_PNG */

/* Does gdImageCreateFromTga work? */
/* #undef HAVE_GD_TGA */

/* Does gdImageCreateFromWebp work? */
/* #undef HAVE_GD_WEBP */

/* Does gdImageCreateFromXpm work? */
/* #undef HAVE_GD_XPM */

/* Define if you have the getaddrinfo function */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the 'getcwd' function. */
#define HAVE_GETCWD 1

/* Define to 1 if you have the 'getgrgid_r' function. */
/* #undef HAVE_GETGRGID_R */

/* Define to 1 if you have the 'getgrnam_r' function. */
#define HAVE_GETGRNAM_R 1

/* Define to 1 if you have the 'getgroups' function. */
/* #undef HAVE_GETGROUPS */

/* */
#define HAVE_GETHOSTBYADDR 1

/* Define to 1 if you have some form of gethostbyname_r(). */
#define HAVE_GETHOSTBYNAME_R 1

/* */
#define HAVE_GETHOSTNAME 1

/* whether getifaddrs is present and usable */
/* #undef HAVE_GETIFADDRS -- not available on this target */

/* Define to 1 if you have the 'getloadavg' function. */
#define HAVE_GETLOADAVG 1

/* Define to 1 if you have the 'getlogin' function. */
#define HAVE_GETLOGIN 1

/* Define to 1 if you have the 'getopt' function. */
#define HAVE_GETOPT 1

/* Define to 1 if you have the 'getpgid' function. */
/* #undef HAVE_GETPGID */

/* Define to 1 if you have the 'getpid' function. */
#define HAVE_GETPID 1

/* Define to 1 if you have the 'getpriority' function. */
/* #undef HAVE_GETPRIORITY */

/* Define to 1 if you have the 'getprotobyname' function. */
/* #undef HAVE_GETPROTOBYNAME -- not available on this target */

/* Define to 1 if you have the 'getprotobynumber' function. */
/* #undef HAVE_GETPROTOBYNUMBER -- not available on this target */

/* Define to 1 if you have the 'getpwnam_r' function. */
#define HAVE_GETPWNAM_R 1

/* Define to 1 if you have the 'getpwuid_r' function. */
#define HAVE_GETPWUID_R 1

/* Define to 1 if you have the 'getrandom' function. */
#define HAVE_GETRANDOM 1

/* Define to 1 if you have the 'getrlimit' function. */
/* #undef HAVE_GETRLIMIT */

/* Define to 1 if you have the 'getrusage' function. */
/* #undef HAVE_GETRUSAGE -- not available on this target */

/* Define to 1 if you have the 'getservbyname' function. */
/* #undef HAVE_GETSERVBYNAME -- not available on this target */

/* Define to 1 if you have the 'getservbyport' function. */
/* #undef HAVE_GETSERVBYPORT -- not available on this target */

/* Define to 1 if you have the 'getsid' function. */
/* #undef HAVE_GETSID */

/* Define to 1 if you have the 'gettid' function. */
#define HAVE_GETTID 1

/* Define to 1 if you have the 'gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the 'getwd' function. */
#define HAVE_GETWD 1

/* glibc's iconv implementation */
/* #undef HAVE_GLIBC_ICONV */

/* Define to 1 if you have the 'glob' function. */
#define HAVE_GLOB 1

/* */
/* #undef HAVE_GMP */

/* Define to 1 if you have the 'gmtime_r' function. */
#define HAVE_GMTIME_R 1

/* Define to 1 if you have the <grp.h> header file. */
#define HAVE_GRP_H 1

/* */
/* #undef HAVE_HISTORY_LIST */

/* Define to 1 if you have the 'hstrerror' function. */
/* #undef HAVE_HSTRERROR */

/* */
#define HAVE_HTONL 1

/* Define to enable copying PHP CODE pages into HUGE PAGES (experimental) */
/* #undef HAVE_HUGE_CODE_PAGES */

/* */
/* #undef HAVE_IBMDB2 */

/* IBM iconv implementation */
/* #undef HAVE_IBM_ICONV */

/* */
/* #undef HAVE_ICONV */

/* Define to 1 if you have the <ieeefp.h> header file. */
/* #undef HAVE_IEEEFP_H */

/* Define to 1 if you have the 'if_indextoname' function. */
/* #undef HAVE_IF_INDEXTONAME */

/* Define to 1 if you have the 'if_nametoindex' function. */
/* #undef HAVE_IF_NAMETOINDEX */

/* */
/* #undef HAVE_IMAP */

/* */
/* #undef HAVE_IMAP2000 */

/* */
/* #undef HAVE_IMAP2001 */

/* */
/* #undef HAVE_IMAP2004 */

/* */
/* #undef HAVE_IMAP_AUTH_GSS */

/* Whether IMAP extension has Kerberos support */
/* #undef HAVE_IMAP_KRB */

/* */
/* #undef HAVE_IMAP_MUTF7 */

/* */
/* #undef HAVE_IMAP_SSL */

/* Define to 1 if you have the <immintrin.h> header file. */
#define HAVE_IMMINTRIN_H 1

/* */
#define HAVE_INET_ATON 1

/* Define to 1 if you have the 'inet_ntoa' function. */
#define HAVE_INET_NTOA 1

/* Define to 1 if you have the 'inet_ntop' function. */
#define HAVE_INET_NTOP 1

/* Define to 1 if you have the 'inet_pton' function. */
#define HAVE_INET_PTON 1

/* Define to 1 if you have the 'initgroups' function. */
/* #undef HAVE_INITGROUPS */

/* Whether intmax_t is available */
#define HAVE_INTMAX_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* */
/* #undef HAVE_IODBC */

/* */
/* #undef HAVE_IODBC_H */

/* Define to 1 if you have the <io.h> header file. */
/* #undef HAVE_IO_H */

/* Whether to enable IPv6 support */
#define HAVE_IPV6 1

/* */
/* #undef HAVE_ISQLEXT_H */

/* */
/* #undef HAVE_ISQL_H */

/* Define to enable JIT */
/* #undef HAVE_JIT */

/* whether to enable JavaScript Object Serialization support */
#define HAVE_JSON 1 

/* Define to 1 if you have the 'kill' function. */
#define HAVE_KILL 1

/* do we have kqueue? */
/* #undef HAVE_KQUEUE */

/* Define to 1 if you have the <langinfo.h> header file. */
#define HAVE_LANGINFO_H 1

/* Define to 1 if you have the 'lchown' function. */
#define HAVE_LCHOWN 1

/* */
/* #undef HAVE_LDAP */

/* Define to 1 if you have the 'ldap_control_find' function. */
/* #undef HAVE_LDAP_CONTROL_FIND */

/* Define to 1 if you have the 'ldap_extended_operation' function. */
/* #undef HAVE_LDAP_EXTENDED_OPERATION */

/* Define to 1 if you have the 'ldap_extended_operation_s' function. */
/* #undef HAVE_LDAP_EXTENDED_OPERATION_S */

/* Define to 1 if you have the 'ldap_parse_extended_result' function. */
/* #undef HAVE_LDAP_PARSE_EXTENDED_RESULT */

/* Define to 1 if you have the 'ldap_parse_reference' function. */
/* #undef HAVE_LDAP_PARSE_REFERENCE */

/* Define to 1 if you have the 'ldap_parse_result' function. */
/* #undef HAVE_LDAP_PARSE_RESULT */

/* Define to 1 if you have the 'ldap_passwd' function. */
/* #undef HAVE_LDAP_PASSWD */

/* Define to 1 if you have the 'ldap_refresh_s' function. */
/* #undef HAVE_LDAP_REFRESH_S */

/* LDAP SASL support */
/* #undef HAVE_LDAP_SASL */

/* Define to 1 if you have the 'ldap_start_tls_s' function. */
/* #undef HAVE_LDAP_START_TLS_S */

/* Define to 1 if you have the 'ldap_whoami_s' function. */
/* #undef HAVE_LDAP_WHOAMI_S */

/* */
/* #undef HAVE_LIBATOMIC */

/* */
/* #undef HAVE_LIBAVIF */

/* */
/* #undef HAVE_LIBBIND */

/* */
/* #undef HAVE_LIBBSD */

/* */
/* #undef HAVE_LIBCRYPT */

/* */
/* #undef HAVE_LIBDL -- no dynamic loading; everything is linked statically */

/* */
/* #undef HAVE_LIBEDIT */

/* */
/* #undef HAVE_LIBEXPAT */

/* */
/* #undef HAVE_LIBFREETYPE */

/* */
/* #undef HAVE_LIBGD */

/* Whether libiconv is used */
/* #undef HAVE_LIBICONV */

/* */
/* #undef HAVE_LIBINTL */

/* */
/* #undef HAVE_LIBJPEG */

/* Define to 1 if you have the 'm' library (-lm). */
#define HAVE_LIBM 1

/* Whether you have libmm */
/* #undef HAVE_LIBMM */

/* */
/* #undef HAVE_LIBNETWORK */

/* */
/* #undef HAVE_LIBNSL */

/* */
/* #undef HAVE_LIBPAM */

/* */
/* #undef HAVE_LIBPNG */

/* Define to 1 if you have the 'pq' library (-lpq). */
/* #undef HAVE_LIBPQ */

/* */
/* #undef HAVE_LIBRARYMANAGER_H */

/* */
/* #undef HAVE_LIBREADLINE */

/* */
/* #undef HAVE_LIBRESOLV */

/* */
/* #undef HAVE_LIBROOT */

/* */
#define HAVE_LIBRT 1

/* */
/* #undef HAVE_LIBSOCKET */

/* */
/* #undef HAVE_LIBSODIUMLIB */

/* */
/* #undef HAVE_LIBUTIL */

/* */
/* #undef HAVE_LIBWEBP */

/* */
/* #undef HAVE_LIBXML */

/* Libzip >= 1.3.1 with zip_libzip_version function */
/* #undef HAVE_LIBZIP_VERSION */

/* Define to 1 if you have the <linux/filter.h> header file. */
#define HAVE_LINUX_FILTER_H 1

/* Define to 1 if you have the <linux/sock_diag.h> header file. */
#define HAVE_LINUX_SOCK_DIAG_H 1

/* Define to 1 if you have the 'localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if the system has the type 'long double'. */
/* #undef HAVE_LONG_DOUBLE */

/* do we have SO_LISTENQxxx? */
/* #undef HAVE_LQ_SO_LISTENQ */

/* do we have TCP_CONNECTION_INFO? */
/* #undef HAVE_LQ_TCP_CONNECTION_INFO */

/* do we have TCP_INFO? */
/* #undef HAVE_LQ_TCP_INFO */

/* do we have mach_vm_read? */
/* #undef HAVE_MACH_VM_READ */

/* Define to 1 if you have the 'makedev' function. */
/* #undef HAVE_MAKEDEV */

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H 1

/* whether to have multibyte regex support */
/* #undef HAVE_MBREGEX */

/* whether to have multibyte string support */
/* #undef HAVE_MBSTRING */

/* Define to 1 if you have the 'memcntl' function. */
/* #undef HAVE_MEMCNTL */

/* Define to 1 if you have the 'memfd_create' function. */
/* #undef HAVE_MEMFD_CREATE */

/* Define to 1 if you have the 'memmem' function. */
#define HAVE_MEMMEM 1

/* Define to 1 if you have the 'memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the 'memrchr' function. */
#define HAVE_MEMRCHR 1

/* Libzip >= 1.7.0 with zip_*_method_supported functions */
/* #undef HAVE_METHOD_SUPPORTED */

/* Define to 1 if you have the <minix/config.h> header file. */
/* #undef HAVE_MINIX_CONFIG_H */

/* Define to 1 if you have the 'mkfifo' function. */
/* #undef HAVE_MKFIFO */

/* Define to 1 if you have the 'mknod' function. */
/* #undef HAVE_MKNOD */

/* Define to 1 if you have the 'mkstemp' function. */
#define HAVE_MKSTEMP 1

/* Define to 1 if you have the 'mmap' function. */
/* #undef HAVE_MMAP -- no mmap here; the allocator uses malloc */

/* Define to 1 if you have the 'mprotect' function. */
/* #undef HAVE_MPROTECT */

/* Define to 1 if you have the 'mremap' function. */
/* #undef HAVE_MREMAP -- no mmap here; the allocator uses malloc */

/* Whether you have MySQL */
/* #undef HAVE_MYSQL */

/* */
#define HAVE_NANOSLEEP 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <netinet/tcp.h> header file. */
/* #undef HAVE_NETINET_TCP_H */

/* Define to 1 if you have the <net/if.h> header file. */
#define HAVE_NET_IF_H 1

/* Whether utf8_mime2text() has new signature */
/* #undef HAVE_NEW_MIME2TEXT */

/* */
/* #undef HAVE_NGETTEXT */

/* Define to 1 if you have the 'nice' function. */
#define HAVE_NICE 1

/* Define to 1 if you have the 'nl_langinfo' function. */
#define HAVE_NL_LANGINFO 1

/* Define to 1 if you have the <nmmintrin.h> header file. */
#define HAVE_NMMINTRIN_H 1

/* Defined to 1 if the PHP OCI8 extension for Oracle Database is configured */
/* #undef HAVE_OCI8 */

/* Defined to 1 if PHP OCI8 DTrace support was enabled during configuration */
/* #undef HAVE_OCI8_DTRACE */

/* */
/* #undef HAVE_OCIENVCREATE */

/* */
/* #undef HAVE_OCIENVNLSCREATE */

/* */
/* #undef HAVE_OCILOBREAD2 */

/* */
/* #undef HAVE_OCISTMTFETCH2 */

/* Defined to 1 if OCI8 configuration located Oracle's Instant Client
   libraries */
/* #undef HAVE_OCI_INSTANT_CLIENT */

/* */
/* #undef HAVE_ODBC2 */

/* */
/* #undef HAVE_ODBCSDK_H */

/* */
/* #undef HAVE_ODBC_H */

/* */
/* #undef HAVE_OPENPTY -- no pseudo-terminals here */

/* Define to 1 if you have the <openssl/crypto.h> header file. */
/* #undef HAVE_OPENSSL_CRYPTO_H */

/* */
/* #undef HAVE_OPENSSL_EXT */

/* */
/* #undef HAVE_ORALDAP */

/* Define to 1 if you have the <os/signpost.h> header file. */
/* #undef HAVE_OS_SIGNPOST_H */

/* Define to 1 if you have the 'pathconf' function. */
/* #undef HAVE_PATHCONF */

/* */
/* #undef HAVE_PCRE_JIT_SUPPORT -- PCRE2 JIT off: no sane W^X story here */

/* */
/* #undef HAVE_PCRE_VALGRIND_SUPPORT */

/* */
/* #undef HAVE_PDO_DBLIB */

/* */
/* #undef HAVE_PDO_FIREBIRD */

/* Whether to build PostgreSQL for PDO support or not */
/* #undef HAVE_PDO_PGSQL */

/* Define to 1 if you have the pdo_sqlite extension enabled. */
/* #undef HAVE_PDO_SQLITELIB */

/* Whether to build PostgreSQL support or not */
/* #undef HAVE_PGSQL */

/* Whether libpq is compiled with --enable-multibyte */
/* #undef HAVE_PGSQL_WITH_MULTIBYTE_SUPPORT */

/* PostgreSQL 9.6 or later */
/* #undef HAVE_PG_CONTEXT_VISIBILITY */

/* PostgreSQL 9.3 or later */
/* #undef HAVE_PG_LO64 */

/* */
/* #undef HAVE_PHPDBG */

/* */
/* #undef HAVE_PHPDBG_READLINE */

/* */
/* #undef HAVE_PHP_SESSION */

/* Define to 1 if you have the 'poll' function. */
/* #undef HAVE_POLL -- no poll(); falls back to select() */

/* Define to 1 if you have the <poll.h> header file. */
/* #undef HAVE_POLL_H -- no poll(); falls back to select() */

/* do we have port framework? */
/* #undef HAVE_PORT */

/* whether to include POSIX-like functions */
/* #undef HAVE_POSIX */

/* */
#define HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR_NP 1

/* do we have prctl? */
/* #undef HAVE_PRCTL -- no prctl() here */

/* */
/* #undef HAVE_PREAD */

/* do we have procctl? */
/* #undef HAVE_PROCCTL */

/* Libzip >= 1.3.0 with zip_register_progress_callback_with_state function */
/* #undef HAVE_PROGRESS_CALLBACK */

/* */
/* #undef HAVE_PSPELL */

/* Define to 1 if the PS_STRINGS thing exists. */
/* #undef HAVE_PS_STRINGS */

/* Define to 1 if you have the 'pthread_attr_getstack' function. */
#define HAVE_PTHREAD_ATTR_GETSTACK 1

/* Define to 1 if you have the 'pthread_attr_get_np' function. */
/* #undef HAVE_PTHREAD_ATTR_GET_NP */

/* Define to 1 if you have the 'pthread_getattr_np' function. */
#define HAVE_PTHREAD_GETATTR_NP 1

/* Define to 1 if you have the 'pthread_get_stackaddr_np' function. */
/* #undef HAVE_PTHREAD_GET_STACKADDR_NP */

/* Define to 1 if you have the 'pthread_jit_write_protect_np' function. */
/* #undef HAVE_PTHREAD_JIT_WRITE_PROTECT_NP */

/* Define to 1 if you have the 'pthread_stackseg_np' function. */
/* #undef HAVE_PTHREAD_STACKSEG_NP */

/* do we have ptrace? */
/* #undef HAVE_PTRACE */

/* Whether ptrdiff_t is available */
#define HAVE_PTRDIFF_T 1

/* Define to 1 if you have the <pty.h> header file. */
/* #undef HAVE_PTY_H -- no pseudo-terminals here */

/* Define to 1 if you have the 'putenv' function. */
#define HAVE_PUTENV 1

/* Define to 1 if you have the <pwd.h> header file. */
#define HAVE_PWD_H 1

/* */
/* #undef HAVE_PWRITE */

/* Define to 1 if you have the 'RAND_egd' function. */
/* #undef HAVE_RAND_EGD */

/* Define to 1 if you have the <resolv.h> header file. */
/* #undef HAVE_RESOLV_H -- no DNS resolver; this board has no networking */

/* */
/* #undef HAVE_RES_NDESTROY */

/* */
/* #undef HAVE_RES_NSEARCH -- no DNS resolver; this board has no networking */

/* */
/* #undef HAVE_RES_SEARCH -- no DNS resolver; this board has no networking */

/* */
/* #undef HAVE_RFC822_OUTPUT_ADDRESS_LIST */

/* Define to 1 if you have the 'rfork' function. */
/* #undef HAVE_RFORK */

/* */
/* #undef HAVE_RL_CALLBACK_READ_CHAR */

/* */
/* #undef HAVE_RL_COMPLETION_MATCHES */

/* */
/* #undef HAVE_RL_ON_NEW_LINE */

/* */
/* #undef HAVE_SAPDB */

/* Whether you have sockaddr_storage.ss_family */
/* #undef HAVE_SA_SS_FAMILY */

/* Define to 1 if you have the 'scandir' function. */
#define HAVE_SCANDIR 1

/* do we have select? */
/* #undef HAVE_SELECT */

/* SElinux available */
/* #undef HAVE_SELINUX */

/* Define to 1 if you have the <selinux/selinux.h> header file. */
/* #undef HAVE_SELINUX_SELINUX_H */

/* */
/* #undef HAVE_SEMUN */

/* Define to 1 if you have the 'setegid' function. */
/* #undef HAVE_SETEGID */

/* Define to 1 if you have the 'setenv' function. */
#define HAVE_SETENV 1

/* Define to 1 if you have the 'seteuid' function. */
/* #undef HAVE_SETEUID */

/* Define to 1 if you have the 'setitimer' function. */
/* #undef HAVE_SETITIMER -- no setitimer/signals; the execution timeout is off anyway */

/* do we have setpflags? */
/* #undef HAVE_SETPFLAGS */

/* Define to 1 if you have the 'setpriority' function. */
/* #undef HAVE_SETPRIORITY */

/* Define to 1 if you have the 'setproctitle' function. */
/* #undef HAVE_SETPROCTITLE */

/* Define to 1 if you have the 'setproctitle_fast' function. */
/* #undef HAVE_SETPROCTITLE_FAST */

/* Define to 1 if you have the 'setrlimit' function. */
/* #undef HAVE_SETRLIMIT */

/* Define to 1 if you have the 'setsid' function. */
/* #undef HAVE_SETSID */

/* */
/* #undef HAVE_SETSOCKOPT */

/* Libzip >= 1.0.0 with zip_file_set_mtime */
/* #undef HAVE_SET_MTIME */

/* */
/* #undef HAVE_SHMOP */

/* Define if you have SysV IPC SHM support */
/* #undef HAVE_SHM_IPC */

/* Define if you have mmap(MAP_ANON) SHM support */
/* #undef HAVE_SHM_MMAP_ANON */

/* Define if you have POSIX mmap() SHM support */
/* #undef HAVE_SHM_MMAP_POSIX */

/* */
/* #undef HAVE_SHM_OPEN */

/* Define to 1 if you have the 'shutdown' function. */
#define HAVE_SHUTDOWN 1

/* */
/* #undef HAVE_SHUTDOWN_SNMP_LOGGING */

/* Define to 1 if you have the 'sigaction' function. */
#define HAVE_SIGACTION 1

/* Define to 1 if you have the 'sigprocmask' function. */
#define HAVE_SIGPROCMASK 1

/* Define to 1 if you have the 'sigsetjmp' function. */
/* #undef HAVE_SIGSETJMP */

/* Define to 1 if you have the 'sigtimedwait' function. */
/* #undef HAVE_SIGTIMEDWAIT */

/* Define to 1 if you have the 'sigwaitinfo' function. */
/* #undef HAVE_SIGWAITINFO */

/* */
/* #undef HAVE_SIMPLEXML */

/* Define if hash3 algo is available */
/* #undef HAVE_SLOW_HASH3 */

/* */
/* #undef HAVE_SNMP */

/* */
/* #undef HAVE_SNMP_SHA256 */

/* */
/* #undef HAVE_SNMP_SHA512 */

/* */
/* #undef HAVE_SOAP */

/* Whether struct sockaddr has field sa_len */
/* #undef HAVE_SOCKADDR_SA_LEN */

/* Whether you have struct sockaddr_storage */
#define HAVE_SOCKADDR_STORAGE 1

/* Define if sockaddr_un in sys/un.h contains a sun_len component */
/* #undef HAVE_SOCKADDR_UN_SUN_LEN */

/* Define to 1 if you have the 'sockatmark' function. */
/* #undef HAVE_SOCKATMARK */

/* */
#define HAVE_SOCKET 1

/* */
#define HAVE_SOCKETPAIR 1

/* */
/* #undef HAVE_SOCKETS */

/* Define to 1 if the system has the type 'socklen_t'. */
#define HAVE_SOCKLEN_T 1

/* */
/* #undef HAVE_SOLID */

/* */
/* #undef HAVE_SOLID_30 */

/* */
/* #undef HAVE_SOLID_35 */

/* */
/* #undef HAVE_SQLCLI1_H */

/* */
/* #undef HAVE_SQLDATASOURCES */

/* */
/* #undef HAVE_SQLEXT_H */

/* Define to 1 if you have the sqlite3 extension enabled. */
/* #undef HAVE_SQLITE3 */

/* have sqlite3_close_v2 */
/* #undef HAVE_SQLITE3_CLOSE_V2 */

/* have sqlite3_column_table_name */
/* #undef HAVE_SQLITE3_COLUMN_TABLE_NAME */

/* have sqlite3_errstr function */
/* #undef HAVE_SQLITE3_ERRSTR */

/* have sqlite3_expanded_sql function */
/* #undef HAVE_SQLITE3_EXPANDED_SQL */

/* */
/* #undef HAVE_SQLTYPES_H */

/* */
/* #undef HAVE_SQLUCODE_H */

/* */
/* #undef HAVE_SQLUNIX_H */

/* */
/* #undef HAVE_SQL_H */

/* Whether ssize_t is available */
#define HAVE_SSIZE_T 1

/* Define to 1 if you have the 'statfs' function. */
/* #undef HAVE_STATFS -- not available on this target */

/* Define to 1 if you have the 'statvfs' function. */
/* #undef HAVE_STATVFS -- not available on this target */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the 'std_syslog' function. */
/* #undef HAVE_STD_SYSLOG */

/* Define to 1 if you have the 'strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define if you have 'strerror_r'. */
#define HAVE_STRERROR_R 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the 'strlcat' function. */
#define HAVE_STRLCAT 1

/* Define to 1 if you have the 'strlcpy' function. */
#define HAVE_STRLCPY 1

/* Define to 1 if you have the 'strndup' function. */
/* #undef HAVE_STRNDUP */

/* Define to 1 if you have the 'strnlen' function. */
#define HAVE_STRNLEN 1

/* Define to 1 if you have the 'strptime' function. */
#define HAVE_STRPTIME 1

/* whether strptime() declaration fails */
#define HAVE_STRPTIME_DECL_FAILS 1

/* Define to 1 if you have the 'strtok_r' function. */
#define HAVE_STRTOK_R 1

/* Define to 1 if you have the 'strtoll' function. */
#define HAVE_STRTOLL 1

/* whether you have struct flock */
#define HAVE_STRUCT_FLOCK 1

/* Define to 1 if 'st_blksize' is a member of 'struct stat'. */
#define HAVE_STRUCT_STAT_ST_BLKSIZE 1

/* Define to 1 if 'st_blocks' is a member of 'struct stat'. */
#define HAVE_STRUCT_STAT_ST_BLOCKS 1

/* Define to 1 if 'st_rdev' is a member of 'struct stat'. */
#define HAVE_STRUCT_STAT_ST_RDEV 1

/* Define to 1 if 'tm_gmtoff' is a member of 'struct tm'. */
#define HAVE_STRUCT_TM_TM_GMTOFF 1

/* Define to 1 if 'tm_zone' is a member of 'struct tm'. */
#define HAVE_STRUCT_TM_TM_ZONE 1

/* Define to 1 if your 'struct stat' has 'st_blocks'. Deprecated, use
   'HAVE_STRUCT_STAT_ST_BLOCKS' instead. */
#define HAVE_ST_BLOCKS 1

/* Define to 1 if you have the 'symlink' function. */
#define HAVE_SYMLINK 1

/* do we have sysconf? */
/* #undef HAVE_SYSCONF */

/* Define to 1 if you have the <sysexits.h> header file. */
/* #undef HAVE_SYSEXITS_H -- not available on this target */

/* Define to 1 if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1 /* provided by compat/syslog.h */

/* FPM use systemd integration */
/* #undef HAVE_SYSTEMD */

/* Define to 1 if you have the <systemd/sd-daemon.h> header file. */
/* #undef HAVE_SYSTEMD_SD_DAEMON_H */

/* */
/* #undef HAVE_SYSVMSG */

/* */
/* #undef HAVE_SYSVSEM */

/* */
/* #undef HAVE_SYSVSHM */

/* Define to 1 if you have the <sys/acl.h> header file. */
/* #undef HAVE_SYS_ACL_H */

/* Define to 1 if you have the <sys/apparmor.h> header file. */
/* #undef HAVE_SYS_APPARMOR_H */

/* Define to 1 if you have the <sys/file.h> header file. */
#define HAVE_SYS_FILE_H 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/ipc.h> header file. */
/* #undef HAVE_SYS_IPC_H -- not available on this target */

/* Define to 1 if you have the <sys/loadavg.h> header file. */
/* #undef HAVE_SYS_LOADAVG_H */

/* Define to 1 if you have the <sys/mkdev.h> header file. */
/* #undef HAVE_SYS_MKDEV_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
/* #undef HAVE_SYS_MMAN_H -- no mmap here; the allocator uses malloc */

/* Define to 1 if you have the <sys/mount.h> header file. */
#define HAVE_SYS_MOUNT_H 1

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/poll.h> header file. */
#define HAVE_SYS_POLL_H 1

/* Define to 1 if you have the <sys/procctl.h> header file. */
/* #undef HAVE_SYS_PROCCTL_H */

/* Define to 1 if you have the <sys/pstat.h> header file. */
/* #undef HAVE_SYS_PSTAT_H */

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have the <sys/sdt.h> header file. */
/* #undef HAVE_SYS_SDT_H */

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/sockio.h> header file. */
/* #undef HAVE_SYS_SOCKIO_H */

/* Define to 1 if you have the <sys/statfs.h> header file. */
/* #undef HAVE_SYS_STATFS_H -- not available on this target */

/* Define to 1 if you have the <sys/statvfs.h> header file. */
#define HAVE_SYS_STATVFS_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/sysexits.h> header file. */
/* #undef HAVE_SYS_SYSEXITS_H */

/* Define to 1 if you have the <sys/sysmacros.h> header file. */
/* #undef HAVE_SYS_SYSMACROS_H */

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/uio.h> header file. */
#define HAVE_SYS_UIO_H 1

/* Define to 1 if you have the <sys/un.h> header file. */
/* #undef HAVE_SYS_UN_H */

/* Define to 1 if you have the <sys/utsname.h> header file. */
/* #undef HAVE_SYS_UTSNAME_H -- not available on this target */

/* Define to 1 if you have the <sys/vfs.h> header file. */
/* #undef HAVE_SYS_VFS_H -- not available on this target */

/* Define to 1 if you have the <sys/wait.h> header file. */
#define HAVE_SYS_WAIT_H 1

/* */
/* #undef HAVE_TIDY */

/* defined if tidybuffio.h exists */
/* #undef HAVE_TIDYBUFFIO_H */

/* Define to 1 if tidyOptGetCategory is available. */
/* #undef HAVE_TIDYOPTGETCATEGORY */

/* */
/* #undef HAVE_TIDYOPTGETDOC */

/* defined if tidyp.h exists */
/* #undef HAVE_TIDYP_H */

/* */
/* #undef HAVE_TIDYRELEASEDATE */

/* defined if tidy.h exists */
/* #undef HAVE_TIDY_H */

/* Have timelib_config.h */
#define HAVE_TIMELIB_CONFIG_H 1

/* */
#define HAVE_TIMER_CREATE 1

/* do we have times? */
/* #undef HAVE_TIMES */

/* Define to 1 if you have the <tmmintrin.h> header file. */
#define HAVE_TMMINTRIN_H 1

/* Define to 1 if your 'struct tm' has 'tm_zone'. Deprecated, use
   'HAVE_STRUCT_TM_TM_ZONE' instead. */
#define HAVE_TM_ZONE 1

/* Whether you have a working ttyname_r */
/* #undef HAVE_TTYNAME_R */

/* Define to 1 if you don't have 'tm_zone' but do have the external array
   'tzname'. */
/* #undef HAVE_TZNAME */

/* Define to 1 if you have the 'tzset' function. */
#define HAVE_TZSET 1

/* */
/* #undef HAVE_UDBCEXT_H */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* */
/* #undef HAVE_UNIXODBC */

/* Define to 1 if you have the <unix.h> header file. */
/* #undef HAVE_UNIX_H */

/* Define to 1 if you have the 'unsetenv' function. */
#define HAVE_UNSETENV 1

/* Define to 1 if you have the 'unshare' function. */
/* #undef HAVE_UNSHARE */

/* */
/* #undef HAVE_UODBC */

/* Whether faulting on write-protected memory support can be compiled for
   userfaultfd */
/* #undef HAVE_USERFAULTFD_WRITEFAULT */

/* Define to 1 if you have the 'usleep' function. */
#define HAVE_USLEEP 1

/* Define to 1 if you have the 'utime' function. */
#define HAVE_UTIME 1

/* Define to 1 if you have the 'utimes' function. */
/* #undef HAVE_UTIMES */

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Whether struct utsname has domainname */
/* #undef HAVE_UTSNAME_DOMAINNAME */

/* */
/* #undef HAVE_VALGRIND */

/* Define to 1 if you have the 'vasprintf' function. */
#define HAVE_VASPRINTF 1

/* Define to 1 if you have the 'wait3' function. */
/* #undef HAVE_WAIT3 */

/* Define to 1 if you have the 'wait4' function. */
/* #undef HAVE_WAIT4 */

/* Define to 1 if you have the 'waitpid' function. */
/* #undef HAVE_WAITPID */

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the <wmmintrin.h> header file. */
#define HAVE_WMMINTRIN_H 1

/* */
/* #undef HAVE_XML */

/* */
/* #undef HAVE_XMLREADER */

/* */
/* #undef HAVE_XMLWRITER */

/* */
/* #undef HAVE_XPM */

/* */
/* #undef HAVE_XSL */

/* */
/* #undef HAVE_XSL_EXSLT */

/* */
/* #undef HAVE_ZIP */

/* */
/* #undef HAVE_ZLIB */

/* whether _controlfp is present usable */
/* #undef HAVE__CONTROLFP */

/* whether _controlfp_s is present and usable */
/* #undef HAVE__CONTROLFP_S */

/* whether _FPU_SETCW is present and usable */
/* #undef HAVE__FPU_SETCW -- no fpu_control.h on this target; the no-op path is used */

/* */
/* #undef HAVE___ATOMIC_EXCHANGE_1 */

/* iconv() is aliased to libiconv() in -liconv */
/* #undef ICONV_ALIASED_LIBICONV */

/* Whether iconv supports IGNORE */
/* #undef ICONV_BROKEN_IGNORE */

/* */
/* #undef JISX0208 */

/* */
/* #undef LMDB_INCLUDE_FILE */

/* Whether asctime_r is declared */
/* #undef MISSING_ASCTIME_R_DECL */

/* Whether ctime_r is declared */
/* #undef MISSING_CTIME_R_DECL */

/* */
#define MISSING_FCLOSE_DECL 0

/* Whether gmtime_r is declared */
/* #undef MISSING_GMTIME_R_DECL */

/* Whether localtime_r is declared */
/* #undef MISSING_LOCALTIME_R_DECL */

/* Whether strtok_r is declared */
/* #undef MISSING_STRTOK_R_DECL */

/* Enable compressed protocol support */
/* #undef MYSQLND_COMPRESSION_ENABLED */

/* Enable mysqlnd code that uses OpenSSL directly */
/* #undef MYSQLND_HAVE_SSL */

/* Enable core mysqlnd SSL code */
/* #undef MYSQLND_SSL_SUPPORTED */

/* */
/* #undef NDBM_INCLUDE_FILE */

/* The highest supported ODBC version */
#define ODBCVER 0x0300

/* Define to the address where bug reports for this package should be sent. */
/* #undef PACKAGE_BUGREPORT */

/* Define to the full name of this package. */
/* #undef PACKAGE_NAME */

/* Define to the full name and version of this package. */
/* #undef PACKAGE_STRING */

/* Define to the one symbol short name of this package. */
/* #undef PACKAGE_TARNAME */

/* Define to the home page for this package. */
/* #undef PACKAGE_URL */

/* Define to the version of this package. */
/* #undef PACKAGE_VERSION */

/* */
#define PCRE2_CODE_UNIT_WIDTH 8

/* */
/* #undef PDO_MYSQL_UNIX_ADDR */

/* Whether pdo_mysql uses mysqlnd */
/* #undef PDO_USE_MYSQLND */

/* */
/* #undef PHAR_HAVE_OPENSSL */

/* */
/* #undef PHPDBG_DEBUG */

/* build architecture */
/* #undef PHP_BUILD_ARCH */

/* used compiler for build */
/* #undef PHP_BUILD_COMPILER */

/* build provider */
/* #undef PHP_BUILD_PROVIDER */

/* builder uname output */
#define PHP_BUILD_SYSTEM "Linux GianfriAur-PC 7.0.12-101.fc43.x86_64 #1 SMP PREEMPT_DYNAMIC Thu Jun 11 01:32:26 UTC 2026 x86_64 GNU/Linux"

/* Define if your system has fork/vfork/CreateProcess */
#define PHP_CAN_SUPPORT_PROC_OPEN 1

/* fpm group name */
/* #undef PHP_FPM_GROUP */

/* fpm systemd service type */
/* #undef PHP_FPM_SYSTEMD */

/* fpm user name */
/* #undef PHP_FPM_USER */

/* Whether the compiler supports AVX512 */
#define PHP_HAVE_AVX512_SUPPORTS 1

/* Whether the compiler supports AVX512 VBMI */
#define PHP_HAVE_AVX512_VBMI_SUPPORTS 1

/* Whether the compiler supports __builtin_clz */
#define PHP_HAVE_BUILTIN_CLZ 1

/* Whether the compiler supports __builtin_clzl */
#define PHP_HAVE_BUILTIN_CLZL 1

/* Whether the compiler supports __builtin_clzll */
#define PHP_HAVE_BUILTIN_CLZLL 1

/* Whether the compiler supports __builtin_cpu_init */
#define PHP_HAVE_BUILTIN_CPU_INIT 1

/* Whether the compiler supports __builtin_cpu_supports */
#define PHP_HAVE_BUILTIN_CPU_SUPPORTS 1

/* Whether the compiler supports __builtin_ctzl */
#define PHP_HAVE_BUILTIN_CTZL 1

/* Whether the compiler supports __builtin_ctzll */
#define PHP_HAVE_BUILTIN_CTZLL 1

/* Whether the compiler supports __builtin_expect */
#define PHP_HAVE_BUILTIN_EXPECT 1

/* Whether the compiler supports __builtin_frame_address */
#define PHP_HAVE_BUILTIN_FRAME_ADDRESS 1

/* Whether the compiler supports __builtin_saddll_overflow */
#define PHP_HAVE_BUILTIN_SADDLL_OVERFLOW 1

/* Whether the compiler supports __builtin_saddl_overflow */
#define PHP_HAVE_BUILTIN_SADDL_OVERFLOW 1

/* Whether the compiler supports __builtin_smulll_overflow */
#define PHP_HAVE_BUILTIN_SMULLL_OVERFLOW 1

/* Whether the compiler supports __builtin_smull_overflow */
#define PHP_HAVE_BUILTIN_SMULL_OVERFLOW 1

/* Whether the compiler supports __builtin_ssubll_overflow */
#define PHP_HAVE_BUILTIN_SSUBLL_OVERFLOW 1

/* Whether the compiler supports __builtin_ssubl_overflow */
#define PHP_HAVE_BUILTIN_SSUBL_OVERFLOW 1

/* Whether the compiler supports __builtin_usub_overflow */
#define PHP_HAVE_BUILTIN_USUB_OVERFLOW 1

/* Whether you have HP-UX 10.x */
/* #undef PHP_HPUX_TIME_R */

/* Which iconv implementation to use */
/* #undef PHP_ICONV_IMPL */

/* Whether you have IRIX-style functions */
/* #undef PHP_IRIX_TIME_R */

/* */
/* #undef PHP_MHASH_BC */

/* */
/* #undef PHP_MYSQL_UNIX_SOCK_ADDR */

/* */
/* #undef PHP_OCI8_DEF_DIR */

/* */
/* #undef PHP_OCI8_DEF_SHARED_LIBADD */

/* define to 1 if oniguruma has an invalid entry for KOI8 encoding */
/* #undef PHP_ONIG_BAD_KOI8_ENTRY */

/* uname output */
#define PHP_OS "Linux"

/* */
/* #undef PHP_PDO_OCI_CLIENT_VERSION */

/* whether pread64 is default */
/* #undef PHP_PREAD_64 */

/* whether pwrite64 is default */
/* #undef PHP_PWRITE_64 */

/* */
#define PHP_SIGCHILD 0

/* uname -a output */
#define PHP_UNAME "Linux GianfriAur-PC 7.0.12-101.fc43.x86_64 #1 SMP PREEMPT_DYNAMIC Thu Jun 11 01:32:26 UTC 2026 x86_64 GNU/Linux"

/* Whether PHP has to use its own crypt_r */
#define PHP_USE_PHP_CRYPT_R 1

/* Use dlopen with RTLD_NOW instead of RTLD_LAZY */
/* #undef PHP_USE_RTLD_NOW */

/* whether write(2) works */
#define PHP_WRITE_STDOUT 1

/* /proc/pid/mem interface */
/* #undef PROC_MEM_FILE */

/* Whether to use Pthreads */
/* #undef PTHREADS */

/* */
/* #undef QDBM_INCLUDE_FILE */

/* */
/* #undef SHADOW_STACK_SYSCALL */

/* The size of 'int', as computed by sizeof. */
#define SIZEOF_INT 4

/* Size of intmax_t */
#define SIZEOF_INTMAX_T 8

/* The size of 'long', as computed by sizeof. */
#define SIZEOF_LONG 4      /* RISC-V32 is ILP32 (was 8 on x86-64) */

/* The size of 'long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The size of 'off_t', as computed by sizeof. */
#define SIZEOF_OFF_T 4     /* RISC-V32 is ILP32 */

/* Size of ptrdiff_t */
#define SIZEOF_PTRDIFF_T 4 /* RISC-V32 is ILP32 */

/* The size of 'short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* The size of 'size_t', as computed by sizeof. */
#define SIZEOF_SIZE_T 4   /* RISC-V32 is ILP32 */

/* Size of ssize_t */
#define SIZEOF_SSIZE_T 4 /* RISC-V32 is ILP32 */

/* have sqlite3 with extension support */
/* #undef SQLITE_OMIT_LOAD_EXTENSION */

/* Needed in sqlunix.h for wchar defs */
/* #undef SS_FBX */

/* Needed in sqlunix.h */
/* #undef SS_LINUX */

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if all of the C89 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Define to 1 if strerror_r returns char *. */
#define STRERROR_R_CHAR_P 1

/* */
/* #undef TCADB_INCLUDE_FILE */

/* Define to 1 if your <sys/time.h> declares 'struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* */
/* #undef USE_GD_JISX0208 */

/* Define if cross-process locking is required by accept() */
/* #undef USE_LOCKING */

/* Use system default cipher list instead of hardcoded value */
/* #undef USE_OPENSSL_SYSTEM_CIPHERS */

/* Enable extensions on AIX, Interix, z/OS.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable general extensions on macOS.  */
#ifndef _DARWIN_C_SOURCE
# define _DARWIN_C_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable X/Open compliant socket functions that do not require linking
   with -lxnet on HP-UX 11.11.  */
#ifndef _HPUX_ALT_XOPEN_SOCKET_API
# define _HPUX_ALT_XOPEN_SOCKET_API 1
#endif
/* Identify the host operating system as Minix.
   This macro does not affect the system headers' behavior.
   A future release of Autoconf may stop defining this macro.  */
#ifndef _MINIX
/* # undef _MINIX */
#endif
/* Enable general extensions on NetBSD.
   Enable NetBSD compatibility extensions on Minix.  */
#ifndef _NETBSD_SOURCE
# define _NETBSD_SOURCE 1
#endif
/* Enable OpenBSD compatibility extensions on NetBSD.
   Oddly enough, this does nothing on OpenBSD.  */
#ifndef _OPENBSD_SOURCE
# define _OPENBSD_SOURCE 1
#endif
/* Define to 1 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_SOURCE
/* # undef _POSIX_SOURCE */
#endif
/* Define to 2 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_1_SOURCE
/* # undef _POSIX_1_SOURCE */
#endif
/* Enable POSIX-compatible threading on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-5:2014.  */
#ifndef __STDC_WANT_IEC_60559_ATTRIBS_EXT__
# define __STDC_WANT_IEC_60559_ATTRIBS_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-1:2014.  */
#ifndef __STDC_WANT_IEC_60559_BFP_EXT__
# define __STDC_WANT_IEC_60559_BFP_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-2:2015.  */
#ifndef __STDC_WANT_IEC_60559_DFP_EXT__
# define __STDC_WANT_IEC_60559_DFP_EXT__ 1
#endif
/* Enable extensions specified by C23 Annex F.  */
#ifndef __STDC_WANT_IEC_60559_EXT__
# define __STDC_WANT_IEC_60559_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-4:2015.  */
#ifndef __STDC_WANT_IEC_60559_FUNCS_EXT__
# define __STDC_WANT_IEC_60559_FUNCS_EXT__ 1
#endif
/* Enable extensions specified by C23 Annex H and ISO/IEC TS 18661-3:2015.  */
#ifndef __STDC_WANT_IEC_60559_TYPES_EXT__
# define __STDC_WANT_IEC_60559_TYPES_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TR 24731-2:2010.  */
#ifndef __STDC_WANT_LIB_EXT2__
# define __STDC_WANT_LIB_EXT2__ 1
#endif
/* Enable extensions specified by ISO/IEC 24747:2009.  */
#ifndef __STDC_WANT_MATH_SPEC_FUNCS__
# define __STDC_WANT_MATH_SPEC_FUNCS__ 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable X/Open extensions.  Define to 500 only if necessary
   to make mbstate_t available.  */
#ifndef _XOPEN_SOURCE
/* # undef _XOPEN_SOURCE */
#endif


/* Define if processor uses big-endian word */
/* #undef WORDS_BIGENDIAN */

/* Define if checking the stack limit is supported */
#define ZEND_CHECK_STACK_LIMIT 1

/* */
#define ZEND_DEBUG 0

/* */
#define ZEND_FIBER_ASM 1

/* */
/* #undef ZEND_FIBER_UCONTEXT */

/* Use zend max execution timers */
/* #undef ZEND_MAX_EXECUTION_TIMERS */

/* */
#define ZEND_MM_ALIGNMENT (size_t)8

/* */
#define ZEND_MM_ALIGNMENT_LOG2 (size_t)3

/* */
#define ZEND_MM_NEED_EIGHT_BYTE_REALIGNMENT 0

/* Use zend signal handling */
/* #undef ZEND_SIGNALS -- no real POSIX signals under FreeRTOS/newlib */

/* */
/* #undef ZTS */

/* */
/* #undef _XOPEN_SOURCE */

/* Define when using musl libc */
/* #undef __MUSL__ */

/* Define to 'int' if <sys/types.h> doesn't define. */
/* #undef gid_t */

/* */
/* #undef in_addr_t */

/* Define to '__inline__' or '__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to 'unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to 'int' if <sys/types.h> doesn't define. */
/* #undef uid_t */


#include <stdlib.h>

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <string.h>

#endif /* PHP_CONFIG_H */

