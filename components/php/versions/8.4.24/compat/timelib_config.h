/*
 * When ext/date is off (the default), some ext/standard headers still pull in
 * php_date.h -> timelib.h, which expects this file (normally generated under
 * ext/date/lib/). Same content as the generated one: just the types and allocation
 * macros, no timezone data. The handful of date symbols actually called are then
 * stubbed in date_stub.c.
 */
#include <php_config.h>
#include <inttypes.h>
#include <stdint.h>

#include "zend.h"

#define timelib_malloc  emalloc
#define timelib_realloc erealloc
#define timelib_calloc  ecalloc
#define timelib_strdup  estrdup
#define timelib_strndup estrndup
#define timelib_free    efree
