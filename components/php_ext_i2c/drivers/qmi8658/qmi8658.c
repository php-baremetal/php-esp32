/* QMI8658 6-axis IMU. poll block + Sensor\Imu. accel()/gyro() in g and dps, temp() in degC. */
#ifdef PHP_I2C_BUILD
#include "php_i2c.h"
#include "zend_exceptions.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define QMI_WHO_AM_I 0x00
#define QMI_CTRL1    0x02
#define QMI_CTRL2    0x03
#define QMI_CTRL3    0x04
#define QMI_CTRL7    0x08
#define QMI_CTRL8    0x09
#define QMI_TEMP_L   0x33
#define QMI_AX_L     0x35
#define QMI_GX_L     0x3B

#define QMI_ACC_SENS 8192.0   /* LSB/g   at +/-4g   */
#define QMI_GYR_SENS 64.0     /* LSB/dps at +/-512  */

extern const i2c_driver_desc_t qmi8658_driver_desc;

static esp_err_t qmi8658_init(i2c_dev_t *d)
{
    uint8_t id = 0;
    esp_err_t e = i2c_dev_read_reg(d, QMI_WHO_AM_I, &id, 1);
    if (e != ESP_OK) {
        return e;
    }
    if (id != 0x05) {
        return ESP_ERR_NOT_FOUND;
    }
    i2c_dev_write_reg1(d, QMI_CTRL1, 0x40);   /* address auto-increment, little-endian output */
    i2c_dev_write_reg1(d, QMI_CTRL7, 0x00);   /* sensors off while configuring */
    i2c_dev_write_reg1(d, QMI_CTRL8, 0xC0);
    i2c_dev_write_reg1(d, QMI_CTRL2, 0x14);   /* accel +/-4g */
    i2c_dev_write_reg1(d, QMI_CTRL3, 0x54);   /* gyro +/-512 dps */
    i2c_dev_write_reg1(d, QMI_CTRL7, 0x03);   /* enable accel + gyro */
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

static int16_t le16(const uint8_t *p)
{
    return (int16_t) (p[0] | (p[1] << 8));
}

static void read_triplet(zval *ret, i2c_dev_t *d, uint8_t reg, double sens, bool *ok)
{
    uint8_t b[6];
    *ok = (i2c_dev_read_reg(d, reg, b, 6) == ESP_OK);
    array_init_size(ret, 3);
    add_next_index_double(ret, le16(b + 0) / sens);
    add_next_index_double(ret, le16(b + 2) / sens);
    add_next_index_double(ret, le16(b + 4) / sens);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_qmi_ctor, 0, 0, 1)
    ZEND_ARG_OBJ_INFO(0, bus, Baremetal\\I2c\\Bus, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, address, IS_LONG, 0, "0x6B")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, hz, IS_LONG, 0, "400000")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_qmi_vec, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_qmi_temp, 0, 0, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Qmi8658, __construct)
{
    i2c_driver_ctor(INTERNAL_FUNCTION_PARAM_PASSTHRU, &qmi8658_driver_desc);
}

PHP_METHOD(Qmi8658, accel)
{
    ZEND_PARSE_PARAMETERS_NONE();
    i2c_dev_t *d = i2c_device_this(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    bool ok;
    read_triplet(return_value, d, QMI_AX_L, QMI_ACC_SENS, &ok);
    if (!ok) {
        zval_ptr_dtor(return_value);
        zend_throw_exception(zend_ce_exception, "QMI8658 accel read failed", 0);
        RETURN_THROWS();
    }
}

PHP_METHOD(Qmi8658, gyro)
{
    ZEND_PARSE_PARAMETERS_NONE();
    i2c_dev_t *d = i2c_device_this(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    bool ok;
    read_triplet(return_value, d, QMI_GX_L, QMI_GYR_SENS, &ok);
    if (!ok) {
        zval_ptr_dtor(return_value);
        zend_throw_exception(zend_ce_exception, "QMI8658 gyro read failed", 0);
        RETURN_THROWS();
    }
}

PHP_METHOD(Qmi8658, temp)
{
    ZEND_PARSE_PARAMETERS_NONE();
    i2c_dev_t *d = i2c_device_this(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    uint8_t b[2];
    if (i2c_dev_read_reg(d, QMI_TEMP_L, b, 2) != ESP_OK) {
        zend_throw_exception(zend_ce_exception, "QMI8658 temp read failed", 0);
        RETURN_THROWS();
    }
    RETURN_DOUBLE(le16(b) / 256.0);
}

static const zend_function_entry qmi8658_methods[] = {
    PHP_ME(Qmi8658, __construct, arginfo_qmi_ctor, ZEND_ACC_PUBLIC)
    PHP_ME(Qmi8658, accel,       arginfo_qmi_vec,  ZEND_ACC_PUBLIC)
    PHP_ME(Qmi8658, gyro,        arginfo_qmi_vec,  ZEND_ACC_PUBLIC)
    PHP_ME(Qmi8658, temp,        arginfo_qmi_temp, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

const i2c_driver_desc_t qmi8658_driver_desc = {
    .name = "qmi8658",
    .php_class = "Qmi8658",
    .capability = "Baremetal\\Sensor\\Imu",
    .default_addrs = { 0x6B, 0x6A },
    .n_default_addrs = 2,
    .default_hz = 400000,
    .init = qmi8658_init,
    .methods = qmi8658_methods,
    .poll = { .sample = NULL, .sample_size = 12, .max_hz = 1000 },
};
#endif
