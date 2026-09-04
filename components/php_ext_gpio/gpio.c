/*
 * gpio extension: gpio_mode(), gpio_write(), gpio_read(), gpio_available().
 *
 * Timing (delay/sleep) lives in the `sys` extension: sys_delay(), with delay() as an alias.
 */
#include "php.h"
#include "php_gpio.h"
#include "driver/gpio.h"

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_gpio_mode, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, pin, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, mode, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_gpio_write, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, pin, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, level, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_gpio_read, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, pin, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_gpio_available, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

/* gpio_mode(int pin, int mode): void -- mode is GPIO_INPUT or GPIO_OUTPUT. */
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

/* gpio_write(int pin, int level): void */
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

/* gpio_available(): bool -- the extension is compiled in (GPIO has no extra precondition). */
PHP_FUNCTION(gpio_available)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_TRUE;
}

static const zend_function_entry gpio_functions[] = {
    PHP_FE(gpio_mode,      arginfo_gpio_mode)
    PHP_FE(gpio_write,     arginfo_gpio_write)
    PHP_FE(gpio_read,      arginfo_gpio_read)
    PHP_FE(gpio_available, arginfo_gpio_available)
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
    "1.0",
    STANDARD_MODULE_PROPERTIES
};
