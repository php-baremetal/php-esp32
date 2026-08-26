/*
 * Hand-written list of the statically-linked extensions. A normal PHP build has
 * configure generate this; we don't run configure, so it's written by hand. The
 * one difference from a stock build is that date is left out (it pulls in timelib
 * and the timezone database).
 *
 * The list here must match the extensions actually compiled: a name listed but
 * not built fails the link with an unresolved phpext_<name>_ptr.
 */
#include "php.h"
#include "php_main.h"
#include "zend_modules.h"
#include "zend_compile.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "ext/pcre/php_pcre.h"
#include "ext/hash/php_hash.h"
#include "ext/json/php_json.h"
#include "ext/random/php_random.h"
#include "ext/reflection/php_reflection.h"
#include "ext/spl/php_spl.h"
#include "ext/standard/php_standard.h"
#include "ext/uri/php_uri.h"   /* PHP 8.5: ext/standard has ZEND_MOD_REQUIRED("uri") */

/* Our hardware extension, defined in components/php_ext_gpio/. Declared here
 * directly to avoid a circular component dependency; the symbol is resolved at
 * the final link. */
extern zend_module_entry gpio_module_entry;
extern zend_module_entry store_module_entry;
extern zend_module_entry mem_module_entry;

/* Optional real ext/date (DateTime), gated by PHP_EXT_DATE_ENABLED. When off, the
 * core uses compat/date_stub.c instead and no date module is registered. */
#ifdef PHP_EXT_DATE_ENABLED
extern zend_module_entry date_module_entry;
#endif

/* Optional ext/ctype, ext/mbstring and ext/filter, gated by their PHP_EXT_*_ENABLED
 * macros (set from the php component's CMakeLists). Three more of PHP's bundled
 * extensions, ported optionally. mbstring is built without the mb_ereg regex family. */
#ifdef PHP_EXT_CTYPE_ENABLED
extern zend_module_entry ctype_module_entry;
#endif
#ifdef PHP_EXT_MBSTRING_ENABLED
extern zend_module_entry mbstring_module_entry;
#endif
#ifdef PHP_EXT_FILTER_ENABLED
extern zend_module_entry filter_module_entry;
#endif
#ifdef PHP_EXT_TOKENIZER_ENABLED
extern zend_module_entry tokenizer_module_entry;
#endif
#ifdef PHP_EXT_SESSION_ENABLED
extern zend_module_entry session_module_entry;
#endif
/* Optional openssl extension, gated by PHP_EXT_OPENSSL_ENABLED. Same module name whether it's
 * the mbedTLS-backed subset or the real ext/openssl (PHP_EXT_OPENSSL_FULL). */
#ifdef PHP_EXT_OPENSSL_ENABLED
extern zend_module_entry openssl_module_entry;
#endif
/* PHP 8.5: OPcache's ini registration looks its module up in the registry by name, so unlike
 * 8.3/8.4 the module must be registered here (its zend_extension startup still runs via the
 * main.c hook). Gated by PHP_EXT_OPCACHE_ENABLED. */
#ifdef PHP_EXT_OPCACHE_ENABLED
extern zend_module_entry opcache_module_entry;
#endif

/* Optional PDO/SQLite extension, gated by PHP_EXT_SQLITE_ENABLED (set from the
 * php component's CMakeLists when built with -DPHP_EXT_SQLITE=ON). pdo must be
 * registered before pdo_sqlite, which depends on it. */
#ifdef PHP_EXT_SQLITE_ENABLED
extern zend_module_entry pdo_module_entry;
extern zend_module_entry pdo_sqlite_module_entry;
#endif

static zend_module_entry * const php_builtin_extensions[] = {
#ifdef PHP_EXT_DATE_ENABLED
	&date_module_entry,
#endif
	phpext_pcre_ptr,
	phpext_hash_ptr,
	phpext_json_ptr,
	phpext_random_ptr,
	phpext_reflection_ptr,
	phpext_uri_ptr,
	phpext_standard_ptr,
	phpext_spl_ptr,
#ifdef PHP_EXT_CTYPE_ENABLED
	&ctype_module_entry,
#endif
#ifdef PHP_EXT_MBSTRING_ENABLED
	&mbstring_module_entry,
#endif
#ifdef PHP_EXT_FILTER_ENABLED
	&filter_module_entry,
#endif
#ifdef PHP_EXT_TOKENIZER_ENABLED
	&tokenizer_module_entry,
#endif
#ifdef PHP_EXT_SESSION_ENABLED
	&session_module_entry,
#endif
#ifdef PHP_EXT_OPENSSL_ENABLED
	&openssl_module_entry,
#endif
#ifdef PHP_EXT_OPCACHE_ENABLED
	&opcache_module_entry,
#endif
#ifdef PHP_EXT_SQLITE_ENABLED
	&pdo_module_entry,
	&pdo_sqlite_module_entry,
#endif
	&gpio_module_entry,
	&store_module_entry,
	&mem_module_entry,
};

#define EXTCOUNT (sizeof(php_builtin_extensions)/sizeof(zend_module_entry *))

PHPAPI int php_register_internal_extensions(void)
{
	return php_register_extensions(php_builtin_extensions, EXTCOUNT);
}
