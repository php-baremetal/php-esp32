/*
 * i2c extension module: at MINIT it resets the registries and registers Baremetal\I2c\Bus,
 * Baremetal\I2c\Device and the selected driver classes. Empty translation unit unless PHP_I2C_BUILD
 * is set (i.e. the project enabled -DPHP_EXT_I2C=ON).
 */
#ifdef PHP_I2C_BUILD
#include "php_i2c.h"

PHP_MINIT_FUNCTION(i2c)
{
    i2c_registry_init();
    i2c_capabilities_register();
    i2c_bus_class_register();
    i2c_device_class_register();
    i2c_driver_classes_register();
    return SUCCESS;
}

static const zend_function_entry i2c_functions[] = {
    PHP_FE_END
};

zend_module_entry i2c_module_entry = {
    STANDARD_MODULE_HEADER,
    "i2c",
    i2c_functions,
    PHP_MINIT(i2c),
    NULL,   /* MSHUTDOWN */
    NULL,   /* RINIT */
    NULL,   /* RSHUTDOWN */
    NULL,   /* MINFO */
    "1.0",
    STANDARD_MODULE_PROPERTIES
};
#endif
