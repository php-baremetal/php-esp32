/*
 * Minimal PHP extension exposing a reboot-persistent key-value store (backed by NVS), so a
 * script can keep small state across resets. See store.c and docs/store.md.
 */
#ifndef PHP_STORE_H
#define PHP_STORE_H

#include "php.h"

extern zend_module_entry store_module_entry;
#define phpext_store_ptr (&store_module_entry)

#endif /* PHP_STORE_H */
