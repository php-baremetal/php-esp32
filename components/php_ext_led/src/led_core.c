/*
 * Baremetal\Led\Device -- the shared behaviour for every addressable-LED driver: a buffer of pixels
 * backed by ESP-IDF's led_strip (RMT), with pixel/fill/hsv/set/show/off/count. Driver classes
 * (Baremetal\Led\Driver\*) extend this and add only their __construct, which binds a descriptor.
 * Empty translation unit unless PHP_LED_BUILD is set (i.e. the project enabled -DPHP_EXT_LED=ON).
 */
#ifdef PHP_LED_BUILD
#include "php_led.h"
#include "led_strip_rmt.h"
#include "zend_exceptions.h"

zend_class_entry *led_device_ce;
static zend_class_entry *led_cap_ce;          /* Baremetal\Output\Led */
static zend_object_handlers led_handlers;

zend_object *led_object_create(zend_class_entry *ce)
{
    led_object *o = zend_object_alloc(sizeof(led_object), ce);
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &led_handlers;
    o->strip = NULL;
    o->count = 0;
    o->channels = 3;
    return &o->std;
}

static void led_object_free(zend_object *obj)
{
    led_object *o = led_object_from(obj);
    if (o->strip) {
        led_strip_del(o->strip);   /* release the RMT channel */
        o->strip = NULL;
    }
    zend_object_std_dtor(obj);
}

static led_object *this_led(zval *zthis)
{
    led_object *o = led_object_from(Z_OBJ_P(zthis));
    if (!o->strip) {
        zend_throw_exception(zend_ce_exception, "Led\\Device is not initialised", 0);
        return NULL;
    }
    return o;
}

static inline uint8_t clamp8(zend_long v)
{
    if (v < 0)   return 0;
    if (v > 255) return 255;
    return (uint8_t) v;
}

/* Integer HSV -> RGB. h wraps mod 360; s and v are 0..255. */
static void hsv2rgb(zend_long h, zend_long s, zend_long v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    h = ((h % 360) + 360) % 360;
    if (s < 0) s = 0; else if (s > 255) s = 255;
    if (v < 0) v = 0; else if (v > 255) v = 255;

    zend_long region = h / 60;
    zend_long rem = (h - region * 60) * 255 / 60;
    zend_long p = v * (255 - s) / 255;
    zend_long q = v * (255 - s * rem / 255) / 255;
    zend_long t = v * (255 - s * (255 - rem) / 255) / 255;
    zend_long rr, gg, bb;

    switch (region) {
        case 0:  rr = v; gg = t; bb = p; break;
        case 1:  rr = q; gg = v; bb = p; break;
        case 2:  rr = p; gg = v; bb = t; break;
        case 3:  rr = p; gg = q; bb = v; break;
        case 4:  rr = t; gg = p; bb = v; break;
        default: rr = v; gg = p; bb = q; break;
    }
    *r = (uint8_t) rr;
    *g = (uint8_t) gg;
    *b = (uint8_t) bb;
}

/* Write one pixel into the strip buffer (does not flush). A 4-channel strip keeps W at 0. */
static esp_err_t led_put(led_object *o, uint32_t i, uint8_t r, uint8_t g, uint8_t b)
{
    if (o->channels == 4) {
        return led_strip_set_pixel_rgbw(o->strip, i, r, g, b, 0);
    }
    return led_strip_set_pixel(o->strip, i, r, g, b);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_led_pixel, 0, 4, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, i, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, r, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, g, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, b, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_led_fill, 0, 3, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, r, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, g, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, b, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_led_hsv, 0, 4, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, i, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, h, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, s, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, v, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_led_set, 0, 3, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, r, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, g, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, b, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_led_show, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_led_off, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_led_count, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(LedDevice, pixel)
{
    zend_long i, r, g, b;
    ZEND_PARSE_PARAMETERS_START(4, 4)
        Z_PARAM_LONG(i)
        Z_PARAM_LONG(r)
        Z_PARAM_LONG(g)
        Z_PARAM_LONG(b)
    ZEND_PARSE_PARAMETERS_END();
    led_object *o = this_led(ZEND_THIS);
    if (!o) {
        RETURN_THROWS();
    }
    if (i < 0 || (uint32_t) i >= o->count) {
        zend_argument_value_error(1, "is out of range for this strip");
        RETURN_THROWS();
    }
    led_put(o, (uint32_t) i, clamp8(r), clamp8(g), clamp8(b));
}

PHP_METHOD(LedDevice, fill)
{
    zend_long r, g, b;
    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_LONG(r)
        Z_PARAM_LONG(g)
        Z_PARAM_LONG(b)
    ZEND_PARSE_PARAMETERS_END();
    led_object *o = this_led(ZEND_THIS);
    if (!o) {
        RETURN_THROWS();
    }
    for (uint32_t i = 0; i < o->count; i++) {
        led_put(o, i, clamp8(r), clamp8(g), clamp8(b));
    }
}

PHP_METHOD(LedDevice, hsv)
{
    zend_long i, h, s, v;
    ZEND_PARSE_PARAMETERS_START(4, 4)
        Z_PARAM_LONG(i)
        Z_PARAM_LONG(h)
        Z_PARAM_LONG(s)
        Z_PARAM_LONG(v)
    ZEND_PARSE_PARAMETERS_END();
    led_object *o = this_led(ZEND_THIS);
    if (!o) {
        RETURN_THROWS();
    }
    if (i < 0 || (uint32_t) i >= o->count) {
        zend_argument_value_error(1, "is out of range for this strip");
        RETURN_THROWS();
    }
    uint8_t r, g, b;
    hsv2rgb(h, s, v, &r, &g, &b);
    led_put(o, (uint32_t) i, r, g, b);
}

PHP_METHOD(LedDevice, set)
{
    zend_long r, g, b;
    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_LONG(r)
        Z_PARAM_LONG(g)
        Z_PARAM_LONG(b)
    ZEND_PARSE_PARAMETERS_END();
    led_object *o = this_led(ZEND_THIS);
    if (!o) {
        RETURN_THROWS();
    }
    for (uint32_t i = 0; i < o->count; i++) {
        led_put(o, i, clamp8(r), clamp8(g), clamp8(b));
    }
    led_strip_refresh(o->strip);
}

PHP_METHOD(LedDevice, show)
{
    ZEND_PARSE_PARAMETERS_NONE();
    led_object *o = this_led(ZEND_THIS);
    if (!o) {
        RETURN_THROWS();
    }
    led_strip_refresh(o->strip);
}

PHP_METHOD(LedDevice, off)
{
    ZEND_PARSE_PARAMETERS_NONE();
    led_object *o = this_led(ZEND_THIS);
    if (!o) {
        RETURN_THROWS();
    }
    led_strip_clear(o->strip);   /* all pixels to 0 and flush */
}

PHP_METHOD(LedDevice, count)
{
    ZEND_PARSE_PARAMETERS_NONE();
    led_object *o = led_object_from(Z_OBJ_P(ZEND_THIS));
    RETURN_LONG((zend_long) o->count);
}

static const zend_function_entry led_device_methods[] = {
    PHP_ME(LedDevice, pixel, arginfo_led_pixel, ZEND_ACC_PUBLIC)
    PHP_ME(LedDevice, fill,  arginfo_led_fill,  ZEND_ACC_PUBLIC)
    PHP_ME(LedDevice, hsv,   arginfo_led_hsv,   ZEND_ACC_PUBLIC)
    PHP_ME(LedDevice, set,   arginfo_led_set,   ZEND_ACC_PUBLIC)
    PHP_ME(LedDevice, show,  arginfo_led_show,  ZEND_ACC_PUBLIC)
    PHP_ME(LedDevice, off,   arginfo_led_off,   ZEND_ACC_PUBLIC)
    PHP_ME(LedDevice, count, arginfo_led_count, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void led_capability_register(void)
{
    zend_class_entry ce;
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\Output", "Led", NULL);
    led_cap_ce = zend_register_internal_interface(&ce);
}

void led_device_class_register(void)
{
    zend_class_entry ce;
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\Led", "Device", led_device_methods);
    led_device_ce = zend_register_internal_class(&ce);
    led_device_ce->create_object = led_object_create;
    if (led_cap_ce) {
        zend_class_implements(led_device_ce, 1, led_cap_ce);
    }

    memcpy(&led_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    led_handlers.offset = XtOffsetOf(led_object, std);
    led_handlers.free_obj = led_object_free;
}

void led_driver_classes_register(void)
{
    for (size_t i = 0; i < led_driver_count; i++) {
        const led_driver_desc_t *desc = led_drivers[i];
        if (!desc || !desc->php_class) {
            continue;
        }
        char fqn[96];
        int n = snprintf(fqn, sizeof(fqn), "Baremetal\\Led\\Driver\\%s", desc->php_class);
        zend_class_entry ce;
        INIT_CLASS_ENTRY_EX(ce, fqn, n, desc->methods);
        zend_class_entry *dce = zend_register_internal_class_ex(&ce, led_device_ce);
        dce->create_object = led_object_create;

        zend_declare_class_constant_long(dce, "GRB", sizeof("GRB") - 1, LED_ORDER_GRB);
        zend_declare_class_constant_long(dce, "RGB", sizeof("RGB") - 1, LED_ORDER_RGB);
        if (desc->channels == 4) {
            zend_declare_class_constant_long(dce, "GRBW", sizeof("GRBW") - 1, LED_ORDER_GRBW);
            zend_declare_class_constant_long(dce, "RGBW", sizeof("RGBW") - 1, LED_ORDER_RGBW);
        }
    }
}

static led_color_component_format_t order_fmt(int order)
{
    switch (order) {
        case LED_ORDER_RGB:  return (led_color_component_format_t) LED_STRIP_COLOR_COMPONENT_FMT_RGB;
        case LED_ORDER_GRBW: return (led_color_component_format_t) LED_STRIP_COLOR_COMPONENT_FMT_GRBW;
        case LED_ORDER_RGBW: return (led_color_component_format_t) LED_STRIP_COLOR_COMPONENT_FMT_RGBW;
        default:             return (led_color_component_format_t) LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    }
}

void led_ctor(INTERNAL_FUNCTION_PARAMETERS, const led_driver_desc_t *desc)
{
    zend_long pin, count = 1, order = 0;
    zend_bool order_is_null = 1;
    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_LONG(pin)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(count)
        Z_PARAM_LONG_OR_NULL(order, order_is_null)
    ZEND_PARSE_PARAMETERS_END();

    if (pin < 0) {
        zend_argument_value_error(1, "must be a valid GPIO");
        return;
    }
    if (count < 1) {
        zend_argument_value_error(2, "must be at least 1");
        return;
    }
    if (order_is_null) {
        order = desc->default_order;
    }

    led_strip_config_t strip_cfg = {
        .strip_gpio_num = (int) pin,
        .max_leds = (uint32_t) count,
        .led_model = (led_model_t) desc->model,
        .color_component_format = order_fmt((int) order),
        .flags = { .invert_out = false },
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,   /* 0.1 us/tick */
        .flags = { .with_dma = false },
    };

    led_object *o = led_object_from(Z_OBJ_P(ZEND_THIS));
    esp_err_t err = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &o->strip);
    if (err != ESP_OK) {
        o->strip = NULL;
        zend_throw_exception_ex(zend_ce_exception, 0,
            "cannot init %s on GPIO %d: %s", desc->name, (int) pin, esp_err_to_name(err));
        return;
    }
    o->count = (uint32_t) count;
    o->channels = desc->channels;
    led_strip_clear(o->strip);
}
#endif /* PHP_LED_BUILD */
