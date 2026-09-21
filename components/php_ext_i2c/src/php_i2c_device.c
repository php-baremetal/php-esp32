/*
 * Baremetal\I2c\Device -- raw transport over one device handle: probe/read/write/readReg/writeReg.
 * The object is a handle into the device registry; its destructor never touches the wire.
 */
#ifdef PHP_I2C_BUILD
#include "php_i2c.h"
#include "zend_exceptions.h"

#define XFER_TIMEOUT_MS  1000

zend_class_entry *i2c_device_ce;
static zend_object_handlers i2c_device_handlers;

static zend_class_entry *i2c_cap_imu_ce;
static zend_class_entry *i2c_cap_touch_ce;

static zend_object *i2c_device_create(zend_class_entry *ce)
{
    i2c_device_object *o = zend_object_alloc(sizeof(i2c_device_object), ce);
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &i2c_device_handlers;
    o->dev = NULL;
    return &o->std;
}

static void i2c_device_free(zend_object *obj)
{
    zend_object_std_dtor(obj);   /* the registry entry outlives the object */
}

void i2c_device_object_wrap(zval *out, i2c_dev_t *dev)
{
    object_init_ex(out, i2c_device_ce);
    i2c_device_object_from(Z_OBJ_P(out))->dev = dev;
}

i2c_dev_t *i2c_device_this(zval *zthis)
{
    i2c_device_object *o = i2c_device_object_from(Z_OBJ_P(zthis));
    if (!o->dev) {
        zend_throw_exception(zend_ce_exception, "I2c\\Device is not initialised", 0);
        return NULL;
    }
    return o->dev;
}
#define this_dev i2c_device_this

ZEND_BEGIN_ARG_INFO_EX(arginfo_device_ctor, 0, 0, 2)
    ZEND_ARG_OBJ_INFO(0, bus, Baremetal\\I2c\\Bus, 0)
    ZEND_ARG_TYPE_INFO(0, address, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, hz, IS_LONG, 0, "100000")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_probe, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_read, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, length, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_write, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_readreg, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, reg, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_writereg, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, reg, IS_LONG, 0)
    ZEND_ARG_TYPE_MASK(0, value, MAY_BE_STRING|MAY_BE_LONG, NULL)
ZEND_END_ARG_INFO()

/* Device is instantiated through Bus::device(); a direct `new` needs a bus and address. */
PHP_METHOD(I2cDevice, __construct)
{
    zval *zbus;
    zend_long addr, hz = I2C_DEFAULT_HZ;
    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_OBJECT_OF_CLASS(zbus, i2c_bus_ce)
        Z_PARAM_LONG(addr)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(hz)
    ZEND_PARSE_PARAMETERS_END();

    i2c_bus_object *bo = i2c_bus_object_from(Z_OBJ_P(zbus));
    if (!bo->bus) {
        zend_throw_exception(zend_ce_exception, "I2c\\Bus is not initialised", 0);
        RETURN_THROWS();
    }

    esp_err_t err = ESP_OK;
    i2c_dev_t *dev = i2c_dev_get(bo->bus, (uint16_t) addr, (uint32_t) hz, NULL, &err);
    if (!dev) {
        zend_throw_exception_ex(zend_ce_exception, 0,
            "cannot add I2C device 0x%02X: %s", (unsigned) addr, esp_err_to_name(err));
        RETURN_THROWS();
    }
    i2c_device_object_from(Z_OBJ_P(ZEND_THIS))->dev = dev;
}

PHP_METHOD(I2cDevice, probe)
{
    ZEND_PARSE_PARAMETERS_NONE();
    i2c_dev_t *d = this_dev(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    i2c_bus_lock(d->bus);
    esp_err_t err = i2c_master_probe(d->bus->handle, d->addr, XFER_TIMEOUT_MS);
    i2c_bus_unlock(d->bus);
    RETURN_BOOL(err == ESP_OK);
}

PHP_METHOD(I2cDevice, read)
{
    zend_long len;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(len)
    ZEND_PARSE_PARAMETERS_END();
    if (len <= 0) {
        zend_argument_value_error(1, "must be greater than 0");
        RETURN_THROWS();
    }
    i2c_dev_t *d = this_dev(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }

    zend_string *buf = zend_string_alloc(len, 0);
    i2c_bus_lock(d->bus);
    esp_err_t err = i2c_master_receive(d->handle, (uint8_t *) ZSTR_VAL(buf), len, XFER_TIMEOUT_MS);
    i2c_bus_unlock(d->bus);
    if (err != ESP_OK) {
        zend_string_release(buf);
        zend_throw_exception_ex(zend_ce_exception, 0, "I2C read failed: %s", esp_err_to_name(err));
        RETURN_THROWS();
    }
    ZSTR_VAL(buf)[len] = '\0';
    RETURN_STR(buf);
}

PHP_METHOD(I2cDevice, write)
{
    zend_string *data;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(data)
    ZEND_PARSE_PARAMETERS_END();
    i2c_dev_t *d = this_dev(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }

    i2c_bus_lock(d->bus);
    esp_err_t err = i2c_master_transmit(d->handle, (const uint8_t *) ZSTR_VAL(data),
                                        ZSTR_LEN(data), XFER_TIMEOUT_MS);
    i2c_bus_unlock(d->bus);
    if (err != ESP_OK) {
        zend_throw_exception_ex(zend_ce_exception, 0, "I2C write failed: %s", esp_err_to_name(err));
        RETURN_THROWS();
    }
}

PHP_METHOD(I2cDevice, readReg)
{
    zend_long reg, len = 1;
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(reg)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(len)
    ZEND_PARSE_PARAMETERS_END();
    if (len <= 0) {
        zend_argument_value_error(2, "must be greater than 0");
        RETURN_THROWS();
    }
    i2c_dev_t *d = this_dev(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }

    uint8_t r = (uint8_t) reg;
    zend_string *buf = zend_string_alloc(len, 0);
    i2c_bus_lock(d->bus);
    esp_err_t err = i2c_master_transmit_receive(d->handle, &r, 1,
                                                (uint8_t *) ZSTR_VAL(buf), len, XFER_TIMEOUT_MS);
    i2c_bus_unlock(d->bus);
    if (err != ESP_OK) {
        zend_string_release(buf);
        zend_throw_exception_ex(zend_ce_exception, 0,
            "I2C readReg 0x%02X failed: %s", (unsigned) reg, esp_err_to_name(err));
        RETURN_THROWS();
    }
    ZSTR_VAL(buf)[len] = '\0';
    RETURN_STR(buf);
}

PHP_METHOD(I2cDevice, writeReg)
{
    zend_long reg;
    zval *value;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(reg)
        Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();
    i2c_dev_t *d = this_dev(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }

    uint8_t stackbuf[1 + 32];
    uint8_t *buf = stackbuf;
    size_t n;
    zend_string *heap = NULL;

    if (Z_TYPE_P(value) == IS_STRING) {
        n = 1 + Z_STRLEN_P(value);
        if (n > sizeof(stackbuf)) {
            heap = zend_string_alloc(n, 0);
            buf = (uint8_t *) ZSTR_VAL(heap);
        }
        buf[0] = (uint8_t) reg;
        memcpy(buf + 1, Z_STRVAL_P(value), Z_STRLEN_P(value));
    } else {
        n = 2;
        buf[0] = (uint8_t) reg;
        buf[1] = (uint8_t) zval_get_long(value);
    }

    i2c_bus_lock(d->bus);
    esp_err_t err = i2c_master_transmit(d->handle, buf, n, XFER_TIMEOUT_MS);
    i2c_bus_unlock(d->bus);
    if (heap) {
        zend_string_release(heap);
    }
    if (err != ESP_OK) {
        zend_throw_exception_ex(zend_ce_exception, 0,
            "I2C writeReg 0x%02X failed: %s", (unsigned) reg, esp_err_to_name(err));
        RETURN_THROWS();
    }
}

static const zend_function_entry i2c_device_methods[] = {
    PHP_ME(I2cDevice, __construct, arginfo_device_ctor,     ZEND_ACC_PUBLIC)
    PHP_ME(I2cDevice, probe,       arginfo_device_probe,    ZEND_ACC_PUBLIC)
    PHP_ME(I2cDevice, read,        arginfo_device_read,     ZEND_ACC_PUBLIC)
    PHP_ME(I2cDevice, write,       arginfo_device_write,    ZEND_ACC_PUBLIC)
    PHP_ME(I2cDevice, readReg,     arginfo_device_readreg,  ZEND_ACC_PUBLIC)
    PHP_ME(I2cDevice, writeReg,    arginfo_device_writereg, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void i2c_device_class_register(void)
{
    zend_class_entry ce;
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\I2c", "Device", i2c_device_methods);
    i2c_device_ce = zend_register_internal_class(&ce);
    i2c_device_ce->create_object = i2c_device_create;

    memcpy(&i2c_device_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    i2c_device_handlers.offset = XtOffsetOf(i2c_device_object, std);
    i2c_device_handlers.free_obj = i2c_device_free;
}

void i2c_capabilities_register(void)
{
    zend_class_entry ce;
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\Sensor", "Imu", NULL);
    i2c_cap_imu_ce = zend_register_internal_interface(&ce);
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\Input", "Touch", NULL);
    i2c_cap_touch_ce = zend_register_internal_interface(&ce);
}

void i2c_driver_classes_register(void)
{
    for (size_t i = 0; i < i2c_driver_count; i++) {
        const i2c_driver_desc_t *desc = i2c_drivers[i];
        if (!desc || !desc->php_class) {
            continue;
        }
        char fqn[96];
        int n = snprintf(fqn, sizeof(fqn), "Baremetal\\I2c\\Driver\\%s", desc->php_class);
        zend_class_entry ce;
        INIT_CLASS_ENTRY_EX(ce, fqn, n, desc->methods);
        zend_class_entry *dce = zend_register_internal_class_ex(&ce, i2c_device_ce);
        dce->create_object = i2c_device_create;

        if (desc->capability) {
            zend_class_entry *iface = NULL;
            if (i2c_cap_imu_ce && strcmp(desc->capability, "Baremetal\\Sensor\\Imu") == 0) {
                iface = i2c_cap_imu_ce;
            } else if (i2c_cap_touch_ce && strcmp(desc->capability, "Baremetal\\Input\\Touch") == 0) {
                iface = i2c_cap_touch_ce;
            }
            if (iface) {
                zend_class_implements(dce, 1, iface);
            }
        }
    }
}

void i2c_driver_ctor(INTERNAL_FUNCTION_PARAMETERS, const i2c_driver_desc_t *desc)
{
    zval *zbus;
    zend_long addr = -1, hz = 0;
    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_OBJECT_OF_CLASS(zbus, i2c_bus_ce)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(addr)
        Z_PARAM_LONG(hz)
    ZEND_PARSE_PARAMETERS_END();

    i2c_bus_object *bo = i2c_bus_object_from(Z_OBJ_P(zbus));
    if (!bo->bus) {
        zend_throw_exception(zend_ce_exception, "I2c\\Bus is not initialised", 0);
        return;
    }
    if (addr < 0) {
        addr = desc->n_default_addrs ? desc->default_addrs[0] : 0;
    }
    if (hz <= 0) {
        hz = desc->default_hz ? desc->default_hz : I2C_DEFAULT_HZ;
    }

    esp_err_t err = ESP_OK;
    i2c_dev_t *dev = i2c_dev_get(bo->bus, (uint16_t) addr, (uint32_t) hz, desc, &err);
    if (!dev) {
        zend_throw_exception_ex(zend_ce_exception, 0,
            "cannot init %s at 0x%02X: %s", desc->name, (unsigned) addr, esp_err_to_name(err));
        return;
    }
    i2c_device_object_from(Z_OBJ_P(ZEND_THIS))->dev = dev;
}
#endif /* PHP_I2C_BUILD */
