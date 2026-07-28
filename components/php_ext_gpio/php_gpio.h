/*
 * Minimal PHP extension exposing the board's GPIO and a delay() helper, so a
 * script can drive pins directly.
 */
#ifndef PHP_GPIO_H
#define PHP_GPIO_H

#include "php.h"

extern zend_module_entry gpio_module_entry;
#define phpext_gpio_ptr (&gpio_module_entry)

#endif /* PHP_GPIO_H */
