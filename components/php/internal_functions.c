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

/* Our hardware extension, defined in components/php_ext_gpio/. Declared here
 * directly to avoid a circular component dependency; the symbol is resolved at
 * the final link. */
extern zend_module_entry gpio_module_entry;

static zend_module_entry * const php_builtin_extensions[] = {
	phpext_pcre_ptr,
	phpext_hash_ptr,
	phpext_json_ptr,
	phpext_random_ptr,
	phpext_reflection_ptr,
	phpext_standard_ptr,
	phpext_spl_ptr,
	&gpio_module_entry,
};

#define EXTCOUNT (sizeof(php_builtin_extensions)/sizeof(zend_module_entry *))

PHPAPI int php_register_internal_extensions(void)
{
	return php_register_extensions(php_builtin_extensions, EXTCOUNT);
}
