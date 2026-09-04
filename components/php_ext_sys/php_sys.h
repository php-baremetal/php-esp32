/*
 * sys extension: system / runtime helpers -- timing, reboot and reset info, chip and board
 * identity, and memory introspection (the real ESP-IDF heap, which PHP's memory_get_usage() can't
 * report on this port since USE_ZEND_ALLOC=0 puts the Zend heap in PSRAM via malloc).
 */
#ifndef PHP_SYS_H
#define PHP_SYS_H

#include "php.h"

extern zend_module_entry sys_module_entry;
#define phpext_sys_ptr (&sys_module_entry)

#endif /* PHP_SYS_H */
