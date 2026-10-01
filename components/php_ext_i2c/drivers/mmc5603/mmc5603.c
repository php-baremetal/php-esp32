/* MMC5603NJ 3-axis magnetometer. poll block + Sensor\Magnetometer. mag() in uT, heading() in degrees. */
#ifdef PHP_I2C_BUILD
#include "php_i2c.h"
#include "zend_exceptions.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#define MMC_OUT      0x00   /* Xout0; 9 bytes of 20-bit X/Y/Z */
#define MMC_TEMP     0x09
#define MMC_STATUS   0x18
#define MMC_ODR      0x1A
#define MMC_CTRL0    0x1B
#define MMC_CTRL1    0x1C
#define MMC_CTRL2    0x1D
#define MMC_PID      0x39

#define MMC_PID_VAL  0x10
#define MMC_ODR_HZ   100    /* continuous-mode output rate */

#define MMC_ZERO     524288.0   /* 1<<19: 20-bit zero-field midscale */
#define MMC_SCALE    0.00625    /* uT per LSB */
#define RAD2DEG      57.29577951308232

extern const i2c_driver_desc_t mmc5603_driver_desc;

static esp_err_t mmc5603_init(i2c_dev_t *d)
{
    uint8_t id = 0;
    esp_err_t e = i2c_dev_read_reg(d, MMC_PID, &id, 1);
    if (e != ESP_OK) {
        return e;
    }
    if (id != MMC_PID_VAL) {
        return ESP_ERR_NOT_FOUND;
    }
    i2c_dev_write_reg1(d, MMC_CTRL1, 0x80);   /* software reset */
    vTaskDelay(pdMS_TO_TICKS(20));
    i2c_dev_write_reg1(d, MMC_CTRL0, 0x08);   /* SET */
    i2c_dev_write_reg1(d, MMC_CTRL0, 0x10);   /* RESET (degauss) */
    i2c_dev_write_reg1(d, MMC_ODR, MMC_ODR_HZ);
    i2c_dev_write_reg1(d, MMC_CTRL0, 0x80);   /* cmm_freq_en */
    i2c_dev_write_reg1(d, MMC_CTRL2, 0x10);   /* cmm_en: continuous */
    vTaskDelay(pdMS_TO_TICKS(20));
    return ESP_OK;
}

static uint32_t u20(const uint8_t *b, int hi, int mid, int lo)
{
    return ((uint32_t) b[hi] << 12) | ((uint32_t) b[mid] << 4) | ((uint32_t) b[lo] >> 4);
}

static void fill_mag(zval *ret, const uint8_t *b)
{
    array_init_size(ret, 3);
    add_next_index_double(ret, ((double) u20(b, 0, 1, 6) - MMC_ZERO) * MMC_SCALE);
    add_next_index_double(ret, ((double) u20(b, 2, 3, 7) - MMC_ZERO) * MMC_SCALE);
    add_next_index_double(ret, ((double) u20(b, 4, 5, 8) - MMC_ZERO) * MMC_SCALE);
}

static double heading_deg(double x, double y)
{
    double h = atan2(y, x) * RAD2DEG;
    if (h < 0) {
        h += 360.0;
    }
    return h;
}

/* poll block: one read of the 9 output bytes (continuous mode, no trigger). Self-locking. */
static esp_err_t mmc5603_sample(i2c_dev_t *d, void *out)
{
    return i2c_dev_read_reg(d, MMC_OUT, out, 9);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_mmc_ctor, 0, 0, 1)
    ZEND_ARG_OBJ_INFO(0, bus, Baremetal\\I2c\\Bus, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, address, IS_LONG, 0, "0x30")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, hz, IS_LONG, 0, "400000")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mmc_vec, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mmc_double, 0, 0, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Mmc5603, __construct)
{
    i2c_driver_ctor(INTERNAL_FUNCTION_PARAM_PASSTHRU, &mmc5603_driver_desc);
}

PHP_METHOD(Mmc5603, mag)
{
    ZEND_PARSE_PARAMETERS_NONE();
    i2c_dev_t *d = i2c_device_this(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    uint8_t b[9];
    if (i2c_dev_read_reg(d, MMC_OUT, b, 9) != ESP_OK) {
        zend_throw_exception(zend_ce_exception, "MMC5603 mag read failed", 0);
        RETURN_THROWS();
    }
    fill_mag(return_value, b);
}

PHP_METHOD(Mmc5603, heading)
{
    ZEND_PARSE_PARAMETERS_NONE();
    i2c_dev_t *d = i2c_device_this(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    uint8_t b[9];
    if (i2c_dev_read_reg(d, MMC_OUT, b, 9) != ESP_OK) {
        zend_throw_exception(zend_ce_exception, "MMC5603 mag read failed", 0);
        RETURN_THROWS();
    }
    double x = ((double) u20(b, 0, 1, 6) - MMC_ZERO) * MMC_SCALE;
    double y = ((double) u20(b, 2, 3, 7) - MMC_ZERO) * MMC_SCALE;
    RETURN_DOUBLE(heading_deg(x, y));
}

PHP_METHOD(Mmc5603, temp)
{
    ZEND_PARSE_PARAMETERS_NONE();
    i2c_dev_t *d = i2c_device_this(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    i2c_dev_write_reg1(d, MMC_CTRL0, 0x02);   /* trigger temperature measurement */
    uint8_t st = 0;
    for (int i = 0; i < 50; i++) {            /* wait for Meas_T_Done (bit 7), bounded */
        vTaskDelay(pdMS_TO_TICKS(2));
        if (i2c_dev_read_reg(d, MMC_STATUS, &st, 1) == ESP_OK && (st & 0x80)) {
            break;
        }
    }
    uint8_t t = 0;
    if (i2c_dev_read_reg(d, MMC_TEMP, &t, 1) != ESP_OK) {
        zend_throw_exception(zend_ce_exception, "MMC5603 temp read failed", 0);
        RETURN_THROWS();
    }
    RETURN_DOUBLE((double) t * 0.8 - 75.0);
}

/* decode(string $raw): array -- one raw poller sample (9 bytes) into ['mag' => [x,y,z] (uT),
 * 'heading' => degrees]. */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mmc_decode, 0, 1, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, raw, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Mmc5603, decode)
{
    zend_string *raw;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(raw)
    ZEND_PARSE_PARAMETERS_END();
    if (ZSTR_LEN(raw) < 9) {
        zend_argument_value_error(1, "an MMC5603 sample is 9 bytes");
        RETURN_THROWS();
    }
    const uint8_t *b = (const uint8_t *) ZSTR_VAL(raw);
    double x = ((double) u20(b, 0, 1, 6) - MMC_ZERO) * MMC_SCALE;
    double y = ((double) u20(b, 2, 3, 7) - MMC_ZERO) * MMC_SCALE;
    double z = ((double) u20(b, 4, 5, 8) - MMC_ZERO) * MMC_SCALE;
    zval mag;
    array_init_size(&mag, 3);
    add_next_index_double(&mag, x);
    add_next_index_double(&mag, y);
    add_next_index_double(&mag, z);
    array_init(return_value);
    add_assoc_zval(return_value, "mag", &mag);
    add_assoc_double(return_value, "heading", heading_deg(x, y));
}

static const zend_function_entry mmc5603_methods[] = {
    PHP_ME(Mmc5603, __construct, arginfo_mmc_ctor,   ZEND_ACC_PUBLIC)
    PHP_ME(Mmc5603, mag,         arginfo_mmc_vec,    ZEND_ACC_PUBLIC)
    PHP_ME(Mmc5603, heading,     arginfo_mmc_double, ZEND_ACC_PUBLIC)
    PHP_ME(Mmc5603, temp,        arginfo_mmc_double, ZEND_ACC_PUBLIC)
    PHP_ME(Mmc5603, decode,      arginfo_mmc_decode, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

const i2c_driver_desc_t mmc5603_driver_desc = {
    .name = "mmc5603",
    .php_class = "Mmc5603",
    .capability = "Baremetal\\Sensor\\Magnetometer",
    .default_addrs = { 0x30 },
    .n_default_addrs = 1,
    .default_hz = 400000,
    .init = mmc5603_init,
    .methods = mmc5603_methods,
    .poll = { .sample = mmc5603_sample, .sample_size = 9, .max_hz = 200 },
};
#endif
