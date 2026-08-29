/*
 * baremetal_utility extension.
 *
 * Memory introspection that PHP's own functions can't give on this port:
 * USE_ZEND_ALLOC=0 puts the Zend heap in PSRAM via malloc, so memory_get_usage()
 * always reads 0. These functions report the real ESP-IDF heap (heap_caps), which
 * is also more honest -- it counts everything (op_arrays, zvals, buffers), not
 * just PHP's arena.
 *
 *   psram_free()          free PSRAM, bytes
 *   psram_size()          total PSRAM pool, bytes
 *   psram_largest_free()  largest contiguous free PSRAM block, bytes (fragmentation)
 *   heap_free()           free internal RAM, bytes
 *   heap_size()           total internal RAM pool, bytes
 */
#include "php.h"
#include "php_baremetal_utility.h"
#include "esp_heap_caps.h"

ZEND_BEGIN_ARG_INFO_EX(arginfo_bmu_none, 0, 0, 0)
ZEND_END_ARG_INFO()

/* psram_free(): int -- free PSRAM in bytes. */
PHP_FUNCTION(psram_free)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

/* psram_size(): int -- total PSRAM pool in bytes. */
PHP_FUNCTION(psram_size)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
}

/* psram_largest_free(): int -- largest contiguous free PSRAM block in bytes. */
PHP_FUNCTION(psram_largest_free)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
}

/* heap_free(): int -- free internal RAM in bytes. */
PHP_FUNCTION(heap_free)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

/* heap_size(): int -- total internal RAM pool in bytes. */
PHP_FUNCTION(heap_size)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
}

static const zend_function_entry baremetal_utility_functions[] = {
    PHP_FE(psram_free,         arginfo_bmu_none)
    PHP_FE(psram_size,         arginfo_bmu_none)
    PHP_FE(psram_largest_free, arginfo_bmu_none)
    PHP_FE(heap_free,          arginfo_bmu_none)
    PHP_FE(heap_size,          arginfo_bmu_none)
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
    "0.1",
    STANDARD_MODULE_PROPERTIES
};
