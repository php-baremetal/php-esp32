/*
 * Baremetal\I2c\Bus -- an I2C master bus. The constructor is an idempotent lookup keyed by
 * (port, sda, scl): same pins, same underlying bus. device() adds a child device; scan() sweeps the
 * bus under its lock. SYNC ownership only for now; CORE1 is reserved.
 */
#ifdef PHP_I2C_BUILD
#include "php_i2c.h"
#include "zend_exceptions.h"

#define PROBE_TIMEOUT_MS  50

zend_class_entry *i2c_bus_ce;
static zend_object_handlers i2c_bus_handlers;

static zend_object *i2c_bus_create(zend_class_entry *ce)
{
    i2c_bus_object *o = zend_object_alloc(sizeof(i2c_bus_object), ce);
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &i2c_bus_handlers;
    o->bus = NULL;
    return &o->std;
}

static void i2c_bus_free(zend_object *obj)
{
    zend_object_std_dtor(obj);   /* the registry entry outlives the object */
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_bus_ctor, 0, 0, 2)
    ZEND_ARG_TYPE_INFO(0, sda, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, scl, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, port, IS_LONG, 0, "-1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, owner, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_bus_device, 0, 1, Baremetal\\I2c\\Device, 0)
    ZEND_ARG_TYPE_INFO(0, address, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, hz, IS_LONG, 0, "100000")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_bus_scan, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(I2cBus, __construct)
{
    zend_long sda, scl, port = I2C_PORT_AUTO, owner = I2C_OWNER_SYNC;
    ZEND_PARSE_PARAMETERS_START(2, 4)
        Z_PARAM_LONG(sda)
        Z_PARAM_LONG(scl)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(port)
        Z_PARAM_LONG(owner)
    ZEND_PARSE_PARAMETERS_END();

    if (owner == I2C_OWNER_CORE1) {
        zend_throw_exception(zend_ce_exception, "I2cBus::CORE1 ownership is not available yet", 0);
        RETURN_THROWS();
    }
    if (owner != I2C_OWNER_SYNC) {
        zend_argument_value_error(4, "must be I2cBus::SYNC or I2cBus::CORE1");
        RETURN_THROWS();
    }

    esp_err_t err = ESP_OK;
    i2c_bus_t *bus = i2c_bus_get((int) port, (int) sda, (int) scl, (i2c_owner_t) owner, &err);
    if (!bus) {
        zend_throw_exception_ex(zend_ce_exception, 0,
            "cannot open I2C bus (sda=%d scl=%d): %s", (int) sda, (int) scl, esp_err_to_name(err));
        RETURN_THROWS();
    }
    i2c_bus_object_from(Z_OBJ_P(ZEND_THIS))->bus = bus;
}

PHP_METHOD(I2cBus, device)
{
    zend_long addr, hz = I2C_DEFAULT_HZ;
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(addr)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(hz)
    ZEND_PARSE_PARAMETERS_END();

    i2c_bus_object *o = i2c_bus_object_from(Z_OBJ_P(ZEND_THIS));
    if (!o->bus) {
        zend_throw_exception(zend_ce_exception, "I2c\\Bus is not initialised", 0);
        RETURN_THROWS();
    }

    esp_err_t err = ESP_OK;
    i2c_dev_t *dev = i2c_dev_get(o->bus, (uint16_t) addr, (uint32_t) hz, NULL, &err);
    if (!dev) {
        zend_throw_exception_ex(zend_ce_exception, 0,
            "cannot add I2C device 0x%02X: %s", (unsigned) addr, esp_err_to_name(err));
        RETURN_THROWS();
    }
    i2c_device_object_wrap(return_value, dev);
}

PHP_METHOD(I2cBus, scan)
{
    ZEND_PARSE_PARAMETERS_NONE();

    i2c_bus_object *o = i2c_bus_object_from(Z_OBJ_P(ZEND_THIS));
    if (!o->bus) {
        zend_throw_exception(zend_ce_exception, "I2c\\Bus is not initialised", 0);
        RETURN_THROWS();
    }

    array_init(return_value);
    i2c_bus_lock(o->bus);
    for (uint16_t a = I2C_SCAN_FIRST; a <= I2C_SCAN_LAST; a++) {
        if (i2c_master_probe(o->bus->handle, a, PROBE_TIMEOUT_MS) != ESP_OK) {
            continue;
        }
        zval info;
        array_init(&info);
        i2c_dev_t *dev = i2c_dev_find(o->bus, a);
        if (dev && dev->driver && dev->driver->name) {
            add_assoc_string(&info, "driver", dev->driver->name);
        } else {
            add_assoc_null(&info, "driver");
        }
        add_index_zval(return_value, a, &info);
    }
    i2c_bus_unlock(o->bus);
}

static const zend_function_entry i2c_bus_methods[] = {
    PHP_ME(I2cBus, __construct, arginfo_bus_ctor,   ZEND_ACC_PUBLIC)
    PHP_ME(I2cBus, device,      arginfo_bus_device, ZEND_ACC_PUBLIC)
    PHP_ME(I2cBus, scan,        arginfo_bus_scan,   ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void i2c_bus_class_register(void)
{
    zend_class_entry ce;
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\I2c", "Bus", i2c_bus_methods);
    i2c_bus_ce = zend_register_internal_class(&ce);
    i2c_bus_ce->create_object = i2c_bus_create;

    zend_declare_class_constant_long(i2c_bus_ce, "SYNC", sizeof("SYNC") - 1, I2C_OWNER_SYNC);
    zend_declare_class_constant_long(i2c_bus_ce, "CORE1", sizeof("CORE1") - 1, I2C_OWNER_CORE1);

    memcpy(&i2c_bus_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    i2c_bus_handlers.offset = XtOffsetOf(i2c_bus_object, std);
    i2c_bus_handlers.free_obj = i2c_bus_free;
}
#endif /* PHP_I2C_BUILD */
