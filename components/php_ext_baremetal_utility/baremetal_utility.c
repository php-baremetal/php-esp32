/*
 * baremetal_utility extension.
 *
 * Memory introspection that PHP's own functions can't give on this port: USE_ZEND_ALLOC=0 puts the
 * Zend heap in PSRAM via malloc, so memory_get_usage() always reads 0. These functions report the
 * real ESP-IDF heap (heap_caps), which is also more honest -- it counts everything (op_arrays,
 * zvals, buffers), not just PHP's arena.
 *
 *   bm_psram_free(): int          free PSRAM, bytes
 *   bm_psram_size(): int          total PSRAM pool, bytes
 *   bm_psram_largest_free(): int  largest contiguous free PSRAM block, bytes (fragmentation)
 *   bm_heap_free(): int           free internal RAM, bytes
 *   bm_heap_size(): int           total internal RAM pool, bytes
 *   bm_available(): bool          PSRAM is present and mapped
 *
 * The unprefixed psram_ and heap_ names are kept as deprecated aliases of the bm_ functions.
 */
#include "php.h"
#include "php_baremetal_utility.h"
#include "esp_heap_caps.h"

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_bm_int, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_bm_bool, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

/* bm_psram_free(): int -- free PSRAM in bytes. */
PHP_FUNCTION(bm_psram_free)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

/* bm_psram_size(): int -- total PSRAM pool in bytes. */
PHP_FUNCTION(bm_psram_size)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
}

/* bm_psram_largest_free(): int -- largest contiguous free PSRAM block in bytes. */
PHP_FUNCTION(bm_psram_largest_free)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
}

/* bm_heap_free(): int -- free internal RAM in bytes. */
PHP_FUNCTION(bm_heap_free)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

/* bm_heap_size(): int -- total internal RAM pool in bytes. */
PHP_FUNCTION(bm_heap_size)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
}

/* bm_available(): bool -- PSRAM is present and mapped (the whole port relies on it). */
PHP_FUNCTION(bm_available)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0);
}

/* Deprecated aliases: the old unprefixed names delegate to the bm_* handlers above. Registered with
 * PHP_DEP_FE below, so the engine emits E_DEPRECATED on use. */
PHP_FUNCTION(psram_free)         { ZEND_PARSE_PARAMETERS_NONE(); RETURN_LONG((zend_long) heap_caps_get_free_size(MALLOC_CAP_SPIRAM)); }
PHP_FUNCTION(psram_size)         { ZEND_PARSE_PARAMETERS_NONE(); RETURN_LONG((zend_long) heap_caps_get_total_size(MALLOC_CAP_SPIRAM)); }
PHP_FUNCTION(psram_largest_free) { ZEND_PARSE_PARAMETERS_NONE(); RETURN_LONG((zend_long) heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)); }
PHP_FUNCTION(heap_free)          { ZEND_PARSE_PARAMETERS_NONE(); RETURN_LONG((zend_long) heap_caps_get_free_size(MALLOC_CAP_INTERNAL)); }
PHP_FUNCTION(heap_size)          { ZEND_PARSE_PARAMETERS_NONE(); RETURN_LONG((zend_long) heap_caps_get_total_size(MALLOC_CAP_INTERNAL)); }

static const zend_function_entry baremetal_utility_functions[] = {
    PHP_FE(bm_psram_free,         arginfo_bm_int)
    PHP_FE(bm_psram_size,         arginfo_bm_int)
    PHP_FE(bm_psram_largest_free, arginfo_bm_int)
    PHP_FE(bm_heap_free,          arginfo_bm_int)
    PHP_FE(bm_heap_size,          arginfo_bm_int)
    PHP_FE(bm_available,          arginfo_bm_bool)
    /* deprecated aliases (use the bm_* names) */
    PHP_DEP_FE(psram_free,         arginfo_bm_int)
    PHP_DEP_FE(psram_size,         arginfo_bm_int)
    PHP_DEP_FE(psram_largest_free, arginfo_bm_int)
    PHP_DEP_FE(heap_free,          arginfo_bm_int)
    PHP_DEP_FE(heap_size,          arginfo_bm_int)
    PHP_FE_END
};

zend_module_entry baremetal_utility_module_entry = {
    STANDARD_MODULE_HEADER,
    "baremetal_utility",
    baremetal_utility_functions,
    NULL,   /* MINIT */
    NULL,   /* MSHUTDOWN */
    NULL,   /* RINIT */
    NULL,   /* RSHUTDOWN */
    NULL,   /* MINFO */
    "1.0",
    STANDARD_MODULE_PROPERTIES
};
