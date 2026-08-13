/*
 * ssd1306: an SSD1306 128x32 OLED driver as a PHP extension.
 *
 * This is the native counterpart to examples/oled-ssd1306-fps, where the same panel is driven by a
 * pure-PHP bit-banged I2C driver. Here the I2C link (the ESP32 hardware I2C peripheral), the
 * framebuffer and the text rendering all live in C; PHP just calls the ssd1306_* functions. It is a
 * per-project extension: phpflash compiles everything under ./firmware/exts/ into the firmware and
 * main.c registers it after php_embed_init(). The point of the pair is to compare the frame rate.
 *
 * Functions:
 *   ssd1306_begin(int sda, int scl, int addr = 0x3C) : bool   -- bring up I2C + init the panel
 *   ssd1306_present() : bool                                  -- does the panel ACK?
 *   ssd1306_clear()                                           -- clear the framebuffer
 *   ssd1306_pixel(int x, int y)                               -- set one pixel
 *   ssd1306_rect(int x, int y, int w, int h)                  -- fill a rectangle
 *   ssd1306_text(int x, int y, string s)                      -- draw 5x7 text
 *   ssd1306_flush() : bool                                    -- push the framebuffer to the panel
 */
#include "php.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include <string.h>

#define SSD_W       128
#define SSD_H       32
#define SSD_PAGES   (SSD_H / 8)
#define SSD_BUFLEN  (SSD_W * SSD_PAGES)   /* 512 */

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;
static uint16_t s_addr = 0x3C;
static uint8_t  s_buf[SSD_BUFLEN];
static uint8_t  s_tx[SSD_BUFLEN + 1];     /* control byte + framebuffer, for one transfer */

/* 5x7 font, column-major, bit 0 = top row -- the glyphs this demo draws. */
static const struct { char c; uint8_t g[5]; } FONT[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
    {'.', {0x00, 0x00, 0x60, 0x00, 0x00}},
    {':', {0x00, 0x00, 0x14, 0x00, 0x00}},
    {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
    {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
    {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
    {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
    {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
    {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
    {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
    {'S', {0x26, 0x49, 0x49, 0x49, 0x32}},
    {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
    {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
};

static const uint8_t *glyph(char c)
{
    for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); i++) {
        if (FONT[i].c == c) {
            return FONT[i].g;
        }
    }
    return FONT[0].g;   /* space */
}

static inline void set_pixel(int x, int y)
{
    if (x < 0 || x >= SSD_W || y < 0 || y >= SSD_H) {
        return;
    }
    s_buf[((y >> 3) * SSD_W) + x] |= (uint8_t) (1 << (y & 7));
}

/* Send command bytes with the 0x00 control byte. */
static esp_err_t ssd_cmds(const uint8_t *cmds, size_t n)
{
    uint8_t pkt[40];
    if (n + 1 > sizeof(pkt) || !s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    pkt[0] = 0x00;
    memcpy(pkt + 1, cmds, n);
    return i2c_master_transmit(s_dev, pkt, n + 1, 100);
}

static void ssd_init_panel(void)
{
    static const uint8_t init[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x1F, 0xD3, 0x00, 0x40, 0x8D, 0x14,
        0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x02, 0x81, 0x8F, 0xD9, 0xF1,
        0xDB, 0x40, 0xA4, 0xA6, 0x2E, 0xAF,
    };
    ssd_cmds(init, sizeof(init));
}

/* ---- PHP functions ------------------------------------------------------- */

ZEND_BEGIN_ARG_INFO_EX(arginfo_begin, 0, 0, 2)
    ZEND_ARG_INFO(0, sda)
    ZEND_ARG_INFO(0, scl)
    ZEND_ARG_INFO(0, addr)
ZEND_END_ARG_INFO()

PHP_FUNCTION(ssd1306_begin)
{
    zend_long sda, scl, addr = 0x3C;
    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_LONG(sda)
        Z_PARAM_LONG(scl)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(addr)
    ZEND_PARSE_PARAMETERS_END();

    s_addr = (uint16_t) addr;

    if (s_bus == NULL) {
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = -1,                       /* auto-pick a free controller */
            .sda_io_num = (int) sda,
            .scl_io_num = (int) scl,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags = { .enable_internal_pullup = true },
        };
        if (i2c_new_master_bus(&bus_cfg, &s_bus) != ESP_OK) {
            RETURN_FALSE;
        }
    }

    if (s_dev) {
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = s_addr,
        .scl_speed_hz = 400000,               /* I2C fast mode */
    };
    if (i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev) != ESP_OK) {
        RETURN_FALSE;
    }

    memset(s_buf, 0, sizeof(s_buf));
    ssd_init_panel();
    RETURN_TRUE;
}

PHP_FUNCTION(ssd1306_present)
{
    ZEND_PARSE_PARAMETERS_NONE();
    if (!s_bus) {
        RETURN_FALSE;
    }
    RETURN_BOOL(i2c_master_probe(s_bus, s_addr, 50) == ESP_OK);
}

PHP_FUNCTION(ssd1306_clear)
{
    ZEND_PARSE_PARAMETERS_NONE();
    memset(s_buf, 0, sizeof(s_buf));
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_pixel, 0, 0, 2)
    ZEND_ARG_INFO(0, x)
    ZEND_ARG_INFO(0, y)
ZEND_END_ARG_INFO()

PHP_FUNCTION(ssd1306_pixel)
{
    zend_long x, y;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(x)
        Z_PARAM_LONG(y)
    ZEND_PARSE_PARAMETERS_END();
    set_pixel((int) x, (int) y);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_rect, 0, 0, 4)
    ZEND_ARG_INFO(0, x)
    ZEND_ARG_INFO(0, y)
    ZEND_ARG_INFO(0, w)
    ZEND_ARG_INFO(0, h)
ZEND_END_ARG_INFO()

PHP_FUNCTION(ssd1306_rect)
{
    zend_long x, y, w, h;
    ZEND_PARSE_PARAMETERS_START(4, 4)
        Z_PARAM_LONG(x)
        Z_PARAM_LONG(y)
        Z_PARAM_LONG(w)
        Z_PARAM_LONG(h)
    ZEND_PARSE_PARAMETERS_END();
    for (int j = 0; j < (int) h; j++) {
        for (int i = 0; i < (int) w; i++) {
            set_pixel((int) x + i, (int) y + j);
        }
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_text, 0, 0, 3)
    ZEND_ARG_INFO(0, x)
    ZEND_ARG_INFO(0, y)
    ZEND_ARG_INFO(0, s)
ZEND_END_ARG_INFO()

PHP_FUNCTION(ssd1306_text)
{
    zend_long x, y;
    char *s;
    size_t slen;
    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_LONG(x)
        Z_PARAM_LONG(y)
        Z_PARAM_STRING(s, slen)
    ZEND_PARSE_PARAMETERS_END();

    int cx = (int) x;
    for (size_t i = 0; i < slen; i++) {
        const uint8_t *g = glyph(s[i]);
        for (int col = 0; col < 5; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1 << row)) {
                    set_pixel(cx + col, (int) y + row);
                }
            }
        }
        cx += 6;   /* 5px glyph + 1px spacing */
    }
}

PHP_FUNCTION(ssd1306_flush)
{
    ZEND_PARSE_PARAMETERS_NONE();
    if (!s_dev) {
        RETURN_FALSE;
    }
    const uint8_t win[] = { 0x21, 0, SSD_W - 1, 0x22, 0, SSD_PAGES - 1 };
    if (ssd_cmds(win, sizeof(win)) != ESP_OK) {
        RETURN_FALSE;
    }
    s_tx[0] = 0x40;   /* Co=0, D/C#=1 -> data */
    memcpy(s_tx + 1, s_buf, SSD_BUFLEN);
    RETURN_BOOL(i2c_master_transmit(s_dev, s_tx, SSD_BUFLEN + 1, 100) == ESP_OK);
}

static const zend_function_entry ssd1306_functions[] = {
    PHP_FE(ssd1306_begin,   arginfo_begin)
    PHP_FE(ssd1306_present, NULL)
    PHP_FE(ssd1306_clear,   NULL)
    PHP_FE(ssd1306_pixel,   arginfo_pixel)
    PHP_FE(ssd1306_rect,    arginfo_rect)
    PHP_FE(ssd1306_text,    arginfo_text)
    PHP_FE(ssd1306_flush,   NULL)
    PHP_FE_END
};

zend_module_entry ssd1306_module_entry = {
    STANDARD_MODULE_HEADER,
    "ssd1306",
    ssd1306_functions,
    NULL, NULL, NULL, NULL, NULL,
    "0.1",
    STANDARD_MODULE_PROPERTIES,
};
