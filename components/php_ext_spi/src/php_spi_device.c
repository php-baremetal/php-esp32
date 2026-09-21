/*
 * Baremetal\Spi\Device -- raw transport over one SPI device: transfer/write/read. The object is a
 * handle into the device registry; its destructor never touches the wire. Also hosts the capability
 * interface registration (Output\Display) and the driver-class registration shared with the panels.
 */
#ifdef PHP_SPI_BUILD
#include "php_spi.h"
#include "zend_exceptions.h"

zend_class_entry *spi_device_ce;
static zend_object_handlers spi_device_handlers;

static zend_class_entry *spi_cap_display_ce;

static zend_object *spidev_obj_create(zend_class_entry *ce)
{
    spi_device_object *o = zend_object_alloc(sizeof(spi_device_object), ce);
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &spi_device_handlers;
    o->dev = NULL;
    return &o->std;
}

static void spidev_obj_free(zend_object *obj)
{
    zend_object_std_dtor(obj);
}

void spi_device_object_wrap(zval *out, spi_dev_t *dev)
{
    object_init_ex(out, spi_device_ce);
    spi_device_object_from(Z_OBJ_P(out))->dev = dev;
}

spi_dev_t *spi_device_this(zval *zthis)
{
    spi_device_object *o = spi_device_object_from(Z_OBJ_P(zthis));
    if (!o->dev) {
        zend_throw_exception(zend_ce_exception, "Spi\\Device is not initialised", 0);
        return NULL;
    }
    return o->dev;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_device_ctor, 0, 0, 2)
    ZEND_ARG_OBJ_INFO(0, bus, Baremetal\\Spi\\Bus, 0)
    ZEND_ARG_TYPE_INFO(0, cs, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, hz, IS_LONG, 0, "1000000")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, mode, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_transfer, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, tx, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_write, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_read, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, length, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(SpiDevice, __construct)
{
    zval *zbus;
    zend_long cs, hz = SPI_DEFAULT_HZ, mode = 0;
    ZEND_PARSE_PARAMETERS_START(2, 4)
        Z_PARAM_OBJECT_OF_CLASS(zbus, spi_bus_ce)
        Z_PARAM_LONG(cs)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(hz)
        Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();

    spi_bus_object *bo = spi_bus_object_from(Z_OBJ_P(zbus));
    if (!bo->bus) {
        zend_throw_exception(zend_ce_exception, "Spi\\Bus is not initialised", 0);
        RETURN_THROWS();
    }
    esp_err_t err = ESP_OK;
    spi_dev_t *dev = spi_dev_get(bo->bus, (int) cs, (uint32_t) hz, (uint8_t) mode, NULL, &err);
    if (!dev) {
        zend_throw_exception_ex(zend_ce_exception, 0,
            "cannot add SPI device cs=%d: %s", (int) cs, esp_err_to_name(err));
        RETURN_THROWS();
    }
    spi_device_object_from(Z_OBJ_P(ZEND_THIS))->dev = dev;
}

PHP_METHOD(SpiDevice, transfer)
{
    zend_string *tx;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(tx)
    ZEND_PARSE_PARAMETERS_END();
    spi_dev_t *d = spi_device_this(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    zend_string *rx = zend_string_alloc(ZSTR_LEN(tx), 0);
    esp_err_t e = spi_dev_transfer(d, (const uint8_t *) ZSTR_VAL(tx), (uint8_t *) ZSTR_VAL(rx),
                                   ZSTR_LEN(tx));
    if (e != ESP_OK) {
        zend_string_release(rx);
        zend_throw_exception_ex(zend_ce_exception, 0, "SPI transfer failed: %s", esp_err_to_name(e));
        RETURN_THROWS();
    }
    ZSTR_VAL(rx)[ZSTR_LEN(rx)] = '\0';
    RETURN_STR(rx);
}

PHP_METHOD(SpiDevice, write)
{
    zend_string *data;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(data)
    ZEND_PARSE_PARAMETERS_END();
    spi_dev_t *d = spi_device_this(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    esp_err_t e = spi_dev_transfer(d, (const uint8_t *) ZSTR_VAL(data), NULL, ZSTR_LEN(data));
    if (e != ESP_OK) {
        zend_throw_exception_ex(zend_ce_exception, 0, "SPI write failed: %s", esp_err_to_name(e));
        RETURN_THROWS();
    }
}

PHP_METHOD(SpiDevice, read)
{
    zend_long len;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(len)
    ZEND_PARSE_PARAMETERS_END();
    if (len <= 0) {
        zend_argument_value_error(1, "must be greater than 0");
        RETURN_THROWS();
    }
    spi_dev_t *d = spi_device_this(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    zend_string *rx = zend_string_alloc(len, 0);
    memset(ZSTR_VAL(rx), 0, len);
    esp_err_t e = spi_dev_transfer(d, (const uint8_t *) ZSTR_VAL(rx), (uint8_t *) ZSTR_VAL(rx), len);
    if (e != ESP_OK) {
        zend_string_release(rx);
        zend_throw_exception_ex(zend_ce_exception, 0, "SPI read failed: %s", esp_err_to_name(e));
        RETURN_THROWS();
    }
    ZSTR_VAL(rx)[len] = '\0';
    RETURN_STR(rx);
}

static const zend_function_entry spi_device_methods[] = {
    PHP_ME(SpiDevice, __construct, arginfo_device_ctor,     ZEND_ACC_PUBLIC)
    PHP_ME(SpiDevice, transfer,    arginfo_device_transfer, ZEND_ACC_PUBLIC)
    PHP_ME(SpiDevice, write,       arginfo_device_write,    ZEND_ACC_PUBLIC)
    PHP_ME(SpiDevice, read,        arginfo_device_read,     ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void spi_device_class_register(void)
{
    zend_class_entry ce;
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\Spi", "Device", spi_device_methods);
    spi_device_ce = zend_register_internal_class(&ce);
    spi_device_ce->create_object = spidev_obj_create;

    memcpy(&spi_device_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    spi_device_handlers.offset = XtOffsetOf(spi_device_object, std);
    spi_device_handlers.free_obj = spidev_obj_free;
}

void spi_capabilities_register(void)
{
    zend_class_entry ce;
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\Output", "Display", NULL);
    spi_cap_display_ce = zend_register_internal_interface(&ce);
}

void spi_driver_classes_register(void)
{
    for (size_t i = 0; i < spi_driver_count; i++) {
        const spi_driver_desc_t *desc = spi_drivers[i];
        if (!desc || !desc->php_class) {
            continue;
        }
        char fqn[96];
        int n = snprintf(fqn, sizeof(fqn), "Baremetal\\Spi\\Driver\\%s", desc->php_class);
        zend_class_entry ce;
        INIT_CLASS_ENTRY_EX(ce, fqn, n, desc->methods);
        zend_class_entry *dce = zend_register_internal_class_ex(&ce, spi_device_ce);
        dce->create_object = spidev_obj_create;

        if (desc->capability && spi_cap_display_ce &&
            strcmp(desc->capability, "Baremetal\\Output\\Display") == 0) {
            zend_class_implements(dce, 1, spi_cap_display_ce);
        }
    }
}

void spi_driver_ctor(INTERNAL_FUNCTION_PARAMETERS, const spi_driver_desc_t *desc)
{
    zval *zbus;
    zend_long cs = -1, hz = 0;
    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_OBJECT_OF_CLASS(zbus, spi_bus_ce)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(cs)
        Z_PARAM_LONG(hz)
    ZEND_PARSE_PARAMETERS_END();

    spi_bus_object *bo = spi_bus_object_from(Z_OBJ_P(zbus));
    if (!bo->bus) {
        zend_throw_exception(zend_ce_exception, "Spi\\Bus is not initialised", 0);
        return;
    }
    if (cs < 0) {
        cs = desc->default_cs;
    }
    if (hz <= 0) {
        hz = desc->default_hz ? desc->default_hz : SPI_DEFAULT_HZ;
    }
    esp_err_t err = ESP_OK;
    spi_dev_t *dev = spi_dev_get(bo->bus, (int) cs, (uint32_t) hz, desc->default_mode, desc, &err);
    if (!dev) {
        zend_throw_exception_ex(zend_ce_exception, 0,
            "cannot init %s (cs=%d): %s", desc->name, (int) cs, esp_err_to_name(err));
        return;
    }
    spi_device_object_from(Z_OBJ_P(ZEND_THIS))->dev = dev;
}
#endif /* PHP_SPI_BUILD */
