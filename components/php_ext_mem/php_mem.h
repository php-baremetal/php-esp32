/*
 * Minimal PHP extension exposing a volatile in-RAM key-value store, shared across the requests of
 * one boot (the RAM twin of the NVS-backed `store_*`). See mem.c and docs/mem.md.
 */
#ifndef PHP_MEM_H
#define PHP_MEM_H

#include "php.h"

extern zend_module_entry mem_module_entry;
#define phpext_mem_ptr (&mem_module_entry)

#endif /* PHP_MEM_H */
