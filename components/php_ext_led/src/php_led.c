/*
 * led extension module: at MINIT it registers the Baremetal\Output\Led capability, the
 * Baremetal\Led\Device base and the selected driver classes (Baremetal\Led\Driver\*). Empty
 * translation unit unless PHP_LED_BUILD is set (i.e. the project enabled -DPHP_EXT_LED=ON).
 */
#ifdef PHP_LED_BUILD
#include "php_led.h"

PHP_MINIT_FUNCTION(led)
{
    led_capability_register();
    led_device_class_register();
    led_driver_classes_register();
    return SUCCESS;
}

static const zend_function_entry led_functions[] = {
    PHP_FE_END
};

zend_module_entry led_module_entry = {
    STANDARD_MODULE_HEADER,
    "led",
    led_functions,
    PHP_MINIT(led),
    NULL,   /* MSHUTDOWN */
    NULL,   /* RINIT */
    NULL,   /* RSHUTDOWN */
    NULL,   /* MINFO */
    "1.0",
    STANDARD_MODULE_PROPERTIES
};
#endif
