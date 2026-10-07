/*
 * led_driver_desc_t -- the contract a LED controller driver fills in. One descriptor per physical chip
 * (WS2812, SK6812, ...): it names the PHP class, the led_strip model and the colour layout. The shared
 * core (led_core.c) does the rest -- buffer, hsv, flush -- so a driver file is just this table.
 */
#pragma once

#include <stddef.h>

/* PHP function table; only the pointer is used here, so the engine headers are not pulled in. */
typedef struct _zend_function_entry zend_function_entry;

/* Colour byte order on the wire (maps to led_strip's color_component_format at construction). */
enum {
    LED_ORDER_GRB  = 0,   /* standard WS2812 */
    LED_ORDER_RGB  = 1,   /* e.g. the ESP32-S3 onboard LED */
    LED_ORDER_GRBW = 2,   /* standard SK6812 RGBW */
    LED_ORDER_RGBW = 3,
};

typedef struct led_driver_desc {
    const char *name;          /* manifest key, e.g. "ws2812" */
    const char *php_class;     /* class under Baremetal\Led\Driver, e.g. "Ws2812" */
    int         model;         /* led_strip led_model_t (LED_MODEL_WS2812 / LED_MODEL_SK6812) */
    int         channels;      /* 3 (RGB) or 4 (RGBW) */
    int         default_order; /* LED_ORDER_* used when the constructor's $order is null */

    const zend_function_entry *methods;   /* the driver class's own methods (its __construct) */
} led_driver_desc_t;

/* The build generates this table from the selected driver list (see CMakeLists and the manifest).
 * Empty when no drivers are selected. */
extern const led_driver_desc_t *const led_drivers[];
extern const size_t led_driver_count;
