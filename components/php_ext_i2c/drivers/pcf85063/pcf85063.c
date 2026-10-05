/* PCF85063 RTC. now() returns the current date/time; set() writes it. BCD registers. */
#ifdef PHP_I2C_BUILD
#include "php_i2c.h"
#include "zend_exceptions.h"

#define PCF_CTRL1   0x00
#define PCF_SECONDS 0x04   /* SECONDS..YEARS are 7 consecutive BCD registers */

extern const i2c_driver_desc_t pcf85063_driver_desc;

static esp_err_t pcf85063_init(i2c_dev_t *d)
{
    return i2c_dev_write_reg1(d, PCF_CTRL1, 0x00);   /* running, no correction */
}

static int bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t dec2bcd(int v) { return (uint8_t) (((v / 10) << 4) | (v % 10)); }

ZEND_BEGIN_ARG_INFO_EX(arginfo_pcf_ctor, 0, 0, 1)
    ZEND_ARG_OBJ_INFO(0, bus, Baremetal\\I2c\\Bus, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, address, IS_LONG, 0, "0x51")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, hz, IS_LONG, 0, "400000")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcf_now, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcf_set, 0, 6, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, year, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, month, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, day, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, hour, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, minute, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, second, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Pcf85063, __construct)
{
    i2c_driver_ctor(INTERNAL_FUNCTION_PARAM_PASSTHRU, &pcf85063_driver_desc);
}

PHP_METHOD(Pcf85063, now)
{
    ZEND_PARSE_PARAMETERS_NONE();
    i2c_dev_t *d = i2c_device_this(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    uint8_t b[7];
    if (i2c_dev_read_reg(d, PCF_SECONDS, b, 7) != ESP_OK) {
        zend_throw_exception(zend_ce_exception, "PCF85063 read failed", 0);
        RETURN_THROWS();
    }
    int sec  = bcd2dec(b[0] & 0x7F);   /* bit7 = clock-integrity flag */
    int min  = bcd2dec(b[1] & 0x7F);
    int hour = bcd2dec(b[2] & 0x3F);
    int day  = bcd2dec(b[3] & 0x3F);
    int mon  = bcd2dec(b[5] & 0x1F);
    int year = 2000 + bcd2dec(b[6]);

    array_init(return_value);
    add_assoc_long(return_value, "year", year);
    add_assoc_long(return_value, "month", mon);
    add_assoc_long(return_value, "day", day);
    add_assoc_long(return_value, "hour", hour);
    add_assoc_long(return_value, "minute", min);
    add_assoc_long(return_value, "second", sec);
    add_assoc_bool(return_value, "valid", (b[0] & 0x80) == 0);
}

PHP_METHOD(Pcf85063, set)
{
    zend_long y, mo, da, h, mi, s;
    ZEND_PARSE_PARAMETERS_START(6, 6)
        Z_PARAM_LONG(y) Z_PARAM_LONG(mo) Z_PARAM_LONG(da)
        Z_PARAM_LONG(h) Z_PARAM_LONG(mi) Z_PARAM_LONG(s)
    ZEND_PARSE_PARAMETERS_END();
    i2c_dev_t *d = i2c_device_this(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    uint8_t b[7] = {
        dec2bcd((int) s), dec2bcd((int) mi), dec2bcd((int) h),
        dec2bcd((int) da), 0 /* weekday, unused */, dec2bcd((int) mo),
        dec2bcd((int) (y % 100)),
    };
    if (i2c_dev_write_reg(d, PCF_SECONDS, b, 7) != ESP_OK) {
        zend_throw_exception(zend_ce_exception, "PCF85063 write failed", 0);
        RETURN_THROWS();
    }
}

static const zend_function_entry pcf85063_methods[] = {
    PHP_ME(Pcf85063, __construct, arginfo_pcf_ctor, ZEND_ACC_PUBLIC)
    PHP_ME(Pcf85063, now,         arginfo_pcf_now,  ZEND_ACC_PUBLIC)
    PHP_ME(Pcf85063, set,         arginfo_pcf_set,  ZEND_ACC_PUBLIC)
    PHP_FE_END
};

const i2c_driver_desc_t pcf85063_driver_desc = {
    .name = "pcf85063",
    .php_class = "Pcf85063",
    .default_addrs = { 0x51 },
    .n_default_addrs = 1,
    .default_hz = 400000,
    .init = pcf85063_init,
    .methods = pcf85063_methods,
};
#endif
