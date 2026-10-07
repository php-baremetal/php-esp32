/* SK6812 addressable LED (4-channel RGBW, GRBW on the wire). pixel()/fill()/hsv() set RGB; the W
 * channel stays off. */
#ifdef PHP_LED_BUILD
#include "php_led.h"

extern const led_driver_desc_t sk6812_driver_desc;

ZEND_BEGIN_ARG_INFO_EX(arginfo_sk6812_ctor, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, pin, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, count, IS_LONG, 0, "1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, order, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()

PHP_METHOD(Sk6812, __construct)
{
    led_ctor(INTERNAL_FUNCTION_PARAM_PASSTHRU, &sk6812_driver_desc);
}

static const zend_function_entry sk6812_methods[] = {
    PHP_ME(Sk6812, __construct, arginfo_sk6812_ctor, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

const led_driver_desc_t sk6812_driver_desc = {
    .name = "sk6812",
    .php_class = "Sk6812",
    .model = LED_MODEL_SK6812,
    .channels = 4,
    .default_order = LED_ORDER_GRBW,
    .methods = sk6812_methods,
};
#endif
