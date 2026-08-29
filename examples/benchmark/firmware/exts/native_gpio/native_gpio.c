/*
 * native_gpio: the C counterpart to the GPIO toggle timing in index.php.
 *
 * It runs the exact same toggle loop -- gpio_set_level(pin, i & 1) -- but in a C for-loop instead
 * of a PHP one, so the two can be compared side by side. gpio_set_level is the same call gpio_write()
 * makes under the hood, so the gap between them is purely the interpreter's per-call cost: the PHP
 * loop pays opcode dispatch + a userland call on every iteration; this pays neither.
 *
 * A per-project extension: phpflash compiles everything under ./firmware/exts/ into the firmware and
 * main.c registers it after php_embed_init(). No CMakeLists needed; the directory name is the module
 * name, and gpio/esp_timer are already in php_project_exts' common REQUIRES.
 *
 *   native_gpio_toggle_ns(int pin, int iters): float
 *       toggle `pin` `iters` times in C; returns nanoseconds per write.
 */
#include "php.h"
#include "driver/gpio.h"
#include "esp_timer.h"

ZEND_BEGIN_ARG_INFO_EX(arginfo_native_gpio_toggle, 0, 0, 2)
    ZEND_ARG_INFO(0, pin)
    ZEND_ARG_INFO(0, iters)
ZEND_END_ARG_INFO()

PHP_FUNCTION(native_gpio_toggle_ns)
{
    zend_long pin, iters;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(pin)
        Z_PARAM_LONG(iters)
    ZEND_PARSE_PARAMETERS_END();

    if (iters < 1) {
        iters = 1;
    }
    gpio_set_direction((gpio_num_t) pin, GPIO_MODE_INPUT_OUTPUT);

    int64_t t0 = esp_timer_get_time();   /* microseconds */
    for (zend_long i = 0; i < iters; i++) {
        gpio_set_level((gpio_num_t) pin, i & 1);
    }
    int64_t t1 = esp_timer_get_time();

    RETURN_DOUBLE((double) (t1 - t0) * 1000.0 / (double) iters);
}

static const zend_function_entry native_gpio_functions[] = {
    PHP_FE(native_gpio_toggle_ns, arginfo_native_gpio_toggle)
    PHP_FE_END
};

zend_module_entry native_gpio_module_entry = {
    STANDARD_MODULE_HEADER,
    "native_gpio",
    native_gpio_functions,
    NULL, NULL, NULL, NULL, NULL,
    "0.1",
    STANDARD_MODULE_PROPERTIES,
};
