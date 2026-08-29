/*
 * baremetal_utility extension: introspection helpers that PHP itself can't
 * provide on this port. With USE_ZEND_ALLOC=0 the engine's heap lives in PSRAM
 * via malloc, so memory_get_usage() reads 0; these functions expose the real
 * ESP-IDF heap (heap_caps) instead.
 */
#ifndef PHP_BAREMETAL_UTILITY_H
#define PHP_BAREMETAL_UTILITY_H

#include "php.h"

extern zend_module_entry baremetal_utility_module_entry;
#define phpext_baremetal_utility_ptr (&baremetal_utility_module_entry)

#endif /* PHP_BAREMETAL_UTILITY_H */
