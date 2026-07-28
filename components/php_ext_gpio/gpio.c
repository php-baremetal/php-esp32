/*
 * gpio extension: gpio_mode(), gpio_write(), gpio_read(), delay().
 *
 * delay() maps onto vTaskDelay(), never a busy-wait: yielding the core keeps the
 * FreeRTOS watchdog happy, which would otherwise reset the board after a couple of
 * seconds of a hot loop.
 */
#include "php.h"
#include "php_gpio.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

ZEND_BEGIN_ARG_INFO_EX(arginfo_gpio_mode, 0, 0, 2)
    ZEND_ARG_INFO(0, pin)
    ZEND_ARG_INFO(0, mode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gpio_write, 0, 0, 2)
    ZEND_ARG_INFO(0, pin)
    ZEND_ARG_INFO(0, level)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gpio_read, 0, 0, 1)
    ZEND_ARG_INFO(0, pin)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_delay, 0, 0, 1)
    ZEND_ARG_INFO(0, ms)
ZEND_END_ARG_INFO()

/* gpio_mode(int pin, int mode): mode is GPIO_INPUT or GPIO_OUTPUT. */
PHP_FUNCTION(gpio_mode)
{
    zend_long pin, mode;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(pin)
        Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();

    gpio_reset_pin((gpio_num_t) pin);
    /* INPUT_OUTPUT (not plain OUTPUT): keeps the input buffer on, so an output
     * pin can still be read back with gpio_read(). */
    gpio_set_direction((gpio_num_t) pin,
                       mode ? GPIO_MODE_INPUT_OUTPUT : GPIO_MODE_INPUT);
}

/* gpio_write(int pin, int level) */
PHP_FUNCTION(gpio_write)
{
    zend_long pin, level;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(pin)
        Z_PARAM_LONG(level)
    ZEND_PARSE_PARAMETERS_END();

    gpio_set_level((gpio_num_t) pin, level ? 1 : 0);
}

/* gpio_read(int pin): int */
PHP_FUNCTION(gpio_read)
{
    zend_long pin;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(pin)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_LONG(gpio_get_level((gpio_num_t) pin));
}

/* delay(int ms): sleep, yielding the core. */
PHP_FUNCTION(delay)
{
    zend_long ms;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(ms)
    ZEND_PARSE_PARAMETERS_END();

    if (ms < 0) {
        ms = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static const zend_function_entry gpio_functions[] = {
    PHP_FE(gpio_mode,  arginfo_gpio_mode)
    PHP_FE(gpio_write, arginfo_gpio_write)
    PHP_FE(gpio_read,  arginfo_gpio_read)
    PHP_FE(delay,      arginfo_delay)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(gpio)
{
    REGISTER_LONG_CONSTANT("GPIO_INPUT",  0, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("GPIO_OUTPUT", 1, CONST_CS | CONST_PERSISTENT);
    return SUCCESS;
}

zend_module_entry gpio_module_entry = {
    STANDARD_MODULE_HEADER,
    "gpio",
    gpio_functions,
    PHP_MINIT(gpio),
    NULL,   /* MSHUTDOWN */
    NULL,   /* RINIT */
    NULL,   /* RSHUTDOWN */
    NULL,   /* MINFO */
    "0.1",
    STANDARD_MODULE_PROPERTIES
};
