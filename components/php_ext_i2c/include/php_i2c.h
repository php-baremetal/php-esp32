/*
 * Internal glue shared by the i2c extension's C files: the class entries, the object wrappers (each a
 * handle into a registry entry) and the registration entry points called from MINIT.
 */
#pragma once

#include "php.h"
#include "i2c_registry.h"

extern zend_module_entry i2c_module_entry;

extern zend_class_entry *i2c_bus_ce;
extern zend_class_entry *i2c_device_ce;

typedef struct {
    i2c_bus_t   *bus;
    zend_object  std;
} i2c_bus_object;

typedef struct {
    i2c_dev_t   *dev;
    zend_object  std;
} i2c_device_object;

static inline i2c_bus_object *i2c_bus_object_from(zend_object *o)
{
    return (i2c_bus_object *)((char *)o - XtOffsetOf(i2c_bus_object, std));
}

static inline i2c_device_object *i2c_device_object_from(zend_object *o)
{
    return (i2c_device_object *)((char *)o - XtOffsetOf(i2c_device_object, std));
}

void i2c_bus_class_register(void);
void i2c_device_class_register(void);

/* Register the capability interfaces (Baremetal\Sensor\Imu, Baremetal\Input\Touch). */
void i2c_capabilities_register(void);

/* Register the selected driver classes (Baremetal\I2c\Driver\*), each extending Device. */
void i2c_driver_classes_register(void);

/* Build an I2cDevice PHP object around an already-registered device entry, into out. */
void i2c_device_object_wrap(zval *out, i2c_dev_t *dev);

/* The device entry behind $this (a Device or a driver instance); throws and returns NULL if unset. */
i2c_dev_t *i2c_device_this(zval *zthis);

/* Shared driver constructor: (Baremetal\I2c\Bus $bus, int $address = default, int $hz = default).
 * Drivers whose methods table has no __construct inherit Device's; those needing extra parameters
 * (reset/interrupt pins, tuning) supply their own and call this after their own parsing if useful. */
void i2c_driver_ctor(INTERNAL_FUNCTION_PARAMETERS, const i2c_driver_desc_t *desc);
