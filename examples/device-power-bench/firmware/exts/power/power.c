/*
 * power: minimal light/deep sleep primitives for the power bench.
 * Not the final power_save API -- just enough to create the sleep regimes and measure them.
 *
 *   light_sleep(int wake_gpio = -1, int timeout_ms = 0): int
 *       enter light sleep (PSRAM retained); wake on wake_gpio going low (if >= 0) and/or the timer
 *       (if timeout_ms > 0). Blocks until wake; returns the wake cause (esp_sleep_wakeup_cause_t).
 *   deep_sleep(int wake_gpio = -1, int timeout_ms = 0): void
 *       enter deep sleep; wake on wake_gpio low (if >= 0) and/or the timer (if timeout_ms > 0). Never
 *       returns (wake is a full reboot).
 *   wake_cause(): int
 *       the wakeup cause of the last reset (0 = normal power-on, not a sleep wake).
 */
#include "php.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

ZEND_BEGIN_ARG_INFO_EX(arginfo_light_sleep, 0, 0, 0)
    ZEND_ARG_INFO(0, wake_gpio)
    ZEND_ARG_INFO(0, timeout_ms)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_deep_sleep, 0, 0, 0)
    ZEND_ARG_INFO(0, wake_gpio)
    ZEND_ARG_INFO(0, timeout_ms)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_power_void, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(light_sleep)
{
    zend_long pin = -1, timeout_ms = 0;
    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(pin)
        Z_PARAM_LONG(timeout_ms)
    ZEND_PARSE_PARAMETERS_END();

    if (pin >= 0) {
        gpio_set_direction((gpio_num_t) pin, GPIO_MODE_INPUT);
        gpio_wakeup_enable((gpio_num_t) pin, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
    }
    if (timeout_ms > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t) timeout_ms * 1000ULL);
    }

    esp_light_sleep_start();                       /* blocks here until a wake source fires */

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (pin >= 0) {
        gpio_wakeup_disable((gpio_num_t) pin);
    }
    if (timeout_ms > 0) {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    }
    RETURN_LONG((zend_long) cause);
}

PHP_FUNCTION(deep_sleep)
{
    zend_long pin = -1, timeout_ms = 0;
    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(pin)
        Z_PARAM_LONG(timeout_ms)
    ZEND_PARSE_PARAMETERS_END();

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (pin >= 0 && rtc_gpio_is_valid_gpio((gpio_num_t) pin)) {
        rtc_gpio_pullup_en((gpio_num_t) pin);      /* hold it high so ext1 ANY_LOW doesn't self-trigger */
        rtc_gpio_pulldown_dis((gpio_num_t) pin);
        esp_sleep_enable_ext1_wakeup(1ULL << pin, ESP_EXT1_WAKEUP_ANY_LOW);
    }
    if (timeout_ms > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t) timeout_ms * 1000ULL);
    }
    esp_deep_sleep_start();                        /* never returns */
}

PHP_FUNCTION(wake_cause)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) esp_sleep_get_wakeup_cause());
}

static const zend_function_entry power_functions[] = {
    PHP_FE(light_sleep, arginfo_light_sleep)
    PHP_FE(deep_sleep,  arginfo_deep_sleep)
    PHP_FE(wake_cause,  arginfo_power_void)
    PHP_FE_END
};

zend_module_entry power_module_entry = {
    STANDARD_MODULE_HEADER,
    "power",
    power_functions,
    NULL, NULL, NULL, NULL, NULL,
    "0.1",
    STANDARD_MODULE_PROPERTIES,
};
