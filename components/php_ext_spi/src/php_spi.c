/*
 * spi extension module: at MINIT it resets the registries and registers Baremetal\Spi\Bus,
 * Baremetal\Spi\Device, the Output\Display capability and the selected driver classes. Empty
 * translation unit unless PHP_SPI_BUILD is set (the project enabled -DPHP_EXT_SPI=ON).
 */
#ifdef PHP_SPI_BUILD
#include "php_spi.h"

PHP_MINIT_FUNCTION(spi)
{
    spi_registry_init();
    spi_capabilities_register();
    spi_bus_class_register();
    spi_device_class_register();
    spi_driver_classes_register();
    return SUCCESS;
}

static const zend_function_entry spi_functions[] = {
    PHP_FE_END
};

zend_module_entry spi_module_entry = {
    STANDARD_MODULE_HEADER,
    "spi",
    spi_functions,
    PHP_MINIT(spi),
    NULL,   /* MSHUTDOWN */
    NULL,   /* RINIT */
    NULL,   /* RSHUTDOWN */
    NULL,   /* MINFO */
    "1.0",
    STANDARD_MODULE_PROPERTIES
};
#endif
