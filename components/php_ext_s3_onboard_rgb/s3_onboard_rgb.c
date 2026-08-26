/*
 * s3_onboard_rgb: drive the WS2812 RGB LED soldered onto ESP32-S3 dev boards.
 *
 * ESP32-S3 ONLY. The onboard addressable LED is an S3 board feature (the P4 boards
 * have no such LED), so this extension is compiled in only for esp32s3 targets and a
 * build for any other target fails early (see extensions.cmake).
 *
 * The LED is driven straight from the SoC's RMT peripheral -- no external component:
 * a single WS2812 pixel, GRB byte order, ~800 kHz. The data pin is fixed at build
 * time via S3_ONBOARD_RGB_GPIO (default 48; override with
 * [extensions.s3_onboard_rgb] pin = N, which flash-tool passes as -DPHP_S3_RGB_GPIO).
 *
 * PHP API:
 *   s3_onboard_rgb_set(int r, int g, int b)   -- each 0..255
 *   s3_onboard_rgb_hsv(int h, int s, int v)   -- h 0..359, s/v 0..255 (handy for rainbows)
 *   s3_onboard_rgb_off()
 *   s3_onboard_rgb_available(): bool
 */
#ifdef S3_ONBOARD_RGB_BUILD

#include "php.h"
#include "php_s3_onboard_rgb.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef S3_ONBOARD_RGB_GPIO
#define S3_ONBOARD_RGB_GPIO 48
#endif

/* Kept alive across requests, like the engine: the RMT channel is set up once on the
 * first call and reused. */
static rmt_channel_handle_t s_chan = NULL;
static rmt_encoder_handle_t s_encoder = NULL;

static int rgb_ensure_init(void)
{
    if (s_chan) {
        return 0;
    }
    rmt_tx_channel_config_t chan_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = S3_ONBOARD_RGB_GPIO,
        .mem_block_symbols = 64,
        .resolution_hz = 10 * 1000 * 1000,   /* 0.1 us per tick */
        .trans_queue_depth = 4,
    };
    if (rmt_new_tx_channel(&chan_cfg, &s_chan) != ESP_OK) {
        s_chan = NULL;
        return -1;
    }
    /* WS2812 bit timings at 0.1 us/tick: bit0 = 0.3us high + 0.9us low;
     * bit1 = 0.9us high + 0.3us low. MSB first. */
    rmt_bytes_encoder_config_t bytes_cfg = {
        .bit0 = { .level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9 },
        .bit1 = { .level0 = 1, .duration0 = 9, .level1 = 0, .duration1 = 3 },
        .flags = { .msb_first = 1 },
    };
    if (rmt_new_bytes_encoder(&bytes_cfg, &s_encoder) != ESP_OK) {
        return -1;
    }
    if (rmt_enable(s_chan) != ESP_OK) {
        return -1;
    }
    return 0;
}

/* Push a single pixel to the LED (WS2812 wants GRB order). */
static void rgb_show(uint8_t r, uint8_t g, uint8_t b)
{
    if (rgb_ensure_init() != 0) {
        return;
    }
    uint8_t grb[3] = { g, r, b };
    rmt_transmit_config_t tx = { .loop_count = 0 };
    rmt_transmit(s_chan, s_encoder, grb, sizeof(grb), &tx);
    rmt_tx_wait_all_done(s_chan, 100);   /* ms; the frame is ~30 us */
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
    zend_long rem = (h - region * 60) * 255 / 60;          /* 0..255 within the sextant */
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

ZEND_BEGIN_ARG_INFO_EX(arginfo_s3_onboard_rgb_set, 0, 0, 3)
    ZEND_ARG_INFO(0, r)
    ZEND_ARG_INFO(0, g)
    ZEND_ARG_INFO(0, b)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_s3_onboard_rgb_hsv, 0, 0, 3)
    ZEND_ARG_INFO(0, h)
    ZEND_ARG_INFO(0, s)
    ZEND_ARG_INFO(0, v)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_s3_onboard_rgb_none, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(s3_onboard_rgb_set)
{
    zend_long r, g, b;
    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_LONG(r)
        Z_PARAM_LONG(g)
        Z_PARAM_LONG(b)
    ZEND_PARSE_PARAMETERS_END();

    rgb_show(clamp8(r), clamp8(g), clamp8(b));
}

PHP_FUNCTION(s3_onboard_rgb_hsv)
{
    zend_long h, s, v;
    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_LONG(h)
        Z_PARAM_LONG(s)
        Z_PARAM_LONG(v)
    ZEND_PARSE_PARAMETERS_END();

    uint8_t r, g, b;
    hsv2rgb(h, s, v, &r, &g, &b);
    rgb_show(r, g, b);
}

PHP_FUNCTION(s3_onboard_rgb_off)
{
    ZEND_PARSE_PARAMETERS_NONE();
    rgb_show(0, 0, 0);
}

PHP_FUNCTION(s3_onboard_rgb_available)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(1);
}

static const zend_function_entry s3_onboard_rgb_functions[] = {
    PHP_FE(s3_onboard_rgb_set,       arginfo_s3_onboard_rgb_set)
    PHP_FE(s3_onboard_rgb_hsv,       arginfo_s3_onboard_rgb_hsv)
    PHP_FE(s3_onboard_rgb_off,       arginfo_s3_onboard_rgb_none)
    PHP_FE(s3_onboard_rgb_available, arginfo_s3_onboard_rgb_none)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(s3_onboard_rgb)
{
    REGISTER_LONG_CONSTANT("S3_ONBOARD_RGB_PIN", S3_ONBOARD_RGB_GPIO, CONST_CS | CONST_PERSISTENT);
    return SUCCESS;
}

zend_module_entry s3_onboard_rgb_module_entry = {
    STANDARD_MODULE_HEADER,
    "s3_onboard_rgb",
    s3_onboard_rgb_functions,
    PHP_MINIT(s3_onboard_rgb),
    NULL,   /* MSHUTDOWN */
    NULL,   /* RINIT */
    NULL,   /* RSHUTDOWN */
    NULL,   /* MINFO */
    "0.1",
    STANDARD_MODULE_PROPERTIES
};

#else  /* !S3_ONBOARD_RGB_BUILD */
/* Not built for this target/config -- keep a non-empty translation unit. */
typedef int s3_onboard_rgb_unused_t;
#endif /* S3_ONBOARD_RGB_BUILD */
