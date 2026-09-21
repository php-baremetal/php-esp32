/*
 * Internal glue shared by the spi extension's C files: the class entries, the object wrappers (each a
 * handle into a registry entry) and the registration entry points called from MINIT.
 */
#pragma once

#include "php.h"
#include "spi_registry.h"

extern zend_module_entry spi_module_entry;

extern zend_class_entry *spi_bus_ce;
extern zend_class_entry *spi_device_ce;

typedef struct {
    spi_bus_t   *bus;
    zend_object  std;
} spi_bus_object;

typedef struct {
    spi_dev_t   *dev;
    zend_object  std;
} spi_device_object;

static inline spi_bus_object *spi_bus_object_from(zend_object *o)
{
    return (spi_bus_object *)((char *)o - XtOffsetOf(spi_bus_object, std));
}

static inline spi_device_object *spi_device_object_from(zend_object *o)
{
    return (spi_device_object *)((char *)o - XtOffsetOf(spi_device_object, std));
}

void spi_bus_class_register(void);
void spi_device_class_register(void);

/* Register the capability interfaces (Baremetal\Output\Display). */
void spi_capabilities_register(void);

/* Register the selected driver classes (Baremetal\Spi\Driver\*), each extending Device. */
void spi_driver_classes_register(void);

/* The device entry behind $this (a Device or a driver instance); throws and returns NULL if unset. */
spi_dev_t *spi_device_this(zval *zthis);

/* Shared driver constructor: (Baremetal\Spi\Bus $bus, int $cs = default, int $hz = default). */
void spi_driver_ctor(INTERNAL_FUNCTION_PARAMETERS, const spi_driver_desc_t *desc);
