/* ST77916 QSPI panel -- minimal proof-of-concept: bring the panel up and fill it with a colour.
 * No framebuffer/text yet; this only proves the SPI/QSPI + Output\Display path works on hardware. */
#ifdef PHP_SPI_BUILD
#include "php_spi.h"
#include "esp_lcd_st77916.h"
#include "st77916_init_cmds.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "zend_exceptions.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

#define FLUSH_BAND 40

extern const spi_driver_desc_t st77916_driver_desc;

typedef struct {
    esp_lcd_panel_handle_t panel;
    int  width, height;
    int  x_gap, y_gap;
    int  rst, bl;
    bool mirror_x, mirror_y, swap_xy, invert;
    bool inited;
} st_state;

static inline uint16_t sw(uint16_t c) { return (uint16_t) ((c >> 8) | (c << 8)); }

static void st_hw_init(st_state *s, int host, int cs)
{
    gpio_set_direction(s->rst, GPIO_MODE_OUTPUT);
    gpio_set_level(s->rst, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(s->rst, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    /* read register 0x04 at a low clock to pick the panel's init variant */
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = ST77916_PANEL_IO_QSPI_CONFIG(cs, NULL, NULL);
    io_cfg.pclk_hz = 5 * 1000 * 1000;
    if (esp_lcd_new_panel_io_spi(host, &io_cfg, &io) != ESP_OK) {
        return;
    }
    uint8_t rd[4] = {0};
    uint32_t read_cmd = (0x0BULL << 24) | (0x04 << 8);
    esp_lcd_panel_io_rx_param(io, read_cmd, rd, sizeof(rd));
    esp_lcd_panel_io_del(io);
    io = NULL;

    io_cfg.pclk_hz = 40 * 1000 * 1000;
    if (esp_lcd_new_panel_io_spi(host, &io_cfg, &io) != ESP_OK) {
        return;
    }

    st77916_vendor_config_t vendor = { .flags = { .use_qspi_interface = 1 } };
    if (rd[0] == 0x00 && rd[1] == 0x02 && rd[2] == 0x7F && rd[3] == 0x7F) {
        vendor.init_cmds = vendor_specific_init_version_2;
        vendor.init_cmds_size = sizeof(vendor_specific_init_version_2) / sizeof(st77916_lcd_init_cmd_t);
    } else {
        vendor.init_cmds = vendor_specific_init_version_1;
        vendor.init_cmds_size = sizeof(vendor_specific_init_version_1) / sizeof(st77916_lcd_init_cmd_t);
    }
    esp_lcd_panel_dev_config_t dev = {
        .reset_gpio_num = -1,   /* already reset above */
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor,
    };
    if (esp_lcd_new_panel_st77916(io, &dev, &s->panel) != ESP_OK) {
        s->panel = NULL;
        return;
    }
    esp_lcd_panel_reset(s->panel);
    esp_lcd_panel_init(s->panel);
    if (s->swap_xy) {
        esp_lcd_panel_swap_xy(s->panel, true);
    }
    if (s->mirror_x || s->mirror_y) {
        esp_lcd_panel_mirror(s->panel, s->mirror_x, s->mirror_y);
    }
    if (s->x_gap || s->y_gap) {
        esp_lcd_panel_set_gap(s->panel, s->x_gap, s->y_gap);
    }
    if (s->invert) {
        esp_lcd_panel_invert_color(s->panel, true);
    }
    esp_lcd_panel_disp_on_off(s->panel, true);

    if (s->bl >= 0) {
        gpio_set_direction(s->bl, GPIO_MODE_OUTPUT);
        gpio_set_level(s->bl, 1);
    }
}

static st_state *this_state(zval *zthis)
{
    spi_dev_t *d = spi_device_this(zthis);
    if (!d) {
        return NULL;
    }
    return d->state;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_st_ctor, 0, 0, 1)
    ZEND_ARG_OBJ_INFO(0, bus, Baremetal\\Spi\\Bus, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, cs, IS_LONG, 0, "21")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, rst, IS_LONG, 0, "3")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, bl, IS_LONG, 0, "-1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, width, IS_LONG, 0, "360")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, height, IS_LONG, 0, "360")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, x_gap, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, y_gap, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, mirror_x, _IS_BOOL, 0, "false")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, mirror_y, _IS_BOOL, 0, "false")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, swap_xy, _IS_BOOL, 0, "false")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, invert, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_st_fill, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, color, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_st_rgb, 0, 3, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, r, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, g, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, b, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(St77916, __construct)
{
    zval *zbus;
    zend_long cs = 21, rst = 3, bl = -1, width = 360, height = 360, x_gap = 0, y_gap = 0;
    bool mirror_x = 0, mirror_y = 0, swap_xy = 0, invert = 0;
    ZEND_PARSE_PARAMETERS_START(1, 12)
        Z_PARAM_OBJECT_OF_CLASS(zbus, spi_bus_ce)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(cs)
        Z_PARAM_LONG(rst)
        Z_PARAM_LONG(bl)
        Z_PARAM_LONG(width)
        Z_PARAM_LONG(height)
        Z_PARAM_LONG(x_gap)
        Z_PARAM_LONG(y_gap)
        Z_PARAM_BOOL(mirror_x)
        Z_PARAM_BOOL(mirror_y)
        Z_PARAM_BOOL(swap_xy)
        Z_PARAM_BOOL(invert)
    ZEND_PARSE_PARAMETERS_END();

    spi_bus_object *bo = spi_bus_object_from(Z_OBJ_P(zbus));
    if (!bo->bus) {
        zend_throw_exception(zend_ce_exception, "Spi\\Bus is not initialised", 0);
        RETURN_THROWS();
    }

    esp_err_t err = ESP_OK;
    spi_dev_t *dev = spi_dev_reserve(bo->bus, (int) cs, &st77916_driver_desc, &err);
    if (!dev) {
        zend_throw_exception_ex(zend_ce_exception, 0, "cannot reserve st77916: %s", esp_err_to_name(err));
        RETURN_THROWS();
    }
    spi_device_object_from(Z_OBJ_P(ZEND_THIS))->dev = dev;

    st_state *s = dev->state;
    if (!s->inited) {
        s->rst = (int) rst;
        s->bl = (int) bl;
        s->width = (int) width;
        s->height = (int) height;
        s->x_gap = (int) x_gap;
        s->y_gap = (int) y_gap;
        s->mirror_x = mirror_x;
        s->mirror_y = mirror_y;
        s->swap_xy = swap_xy;
        s->invert = invert;
        st_hw_init(s, bo->bus->host, (int) cs);
        if (!s->panel) {
            zend_throw_exception(zend_ce_exception, "st77916 init failed", 0);
            RETURN_THROWS();
        }
        s->inited = true;
    }
}

/* fill(int color): paint the whole panel one RGB565 colour. Sent a band at a time from a small
 * uniform buffer, so no full framebuffer is needed. */
PHP_METHOD(St77916, fill)
{
    zend_long color;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(color)
    ZEND_PARSE_PARAMETERS_END();
    st_state *s = this_state(ZEND_THIS);
    if (!s) {
        RETURN_THROWS();
    }

    size_t band_px = (size_t) s->width * FLUSH_BAND;
    uint16_t *band = malloc(band_px * sizeof(uint16_t));
    if (!band) {
        zend_throw_exception(zend_ce_exception, "st77916 fill: out of memory", 0);
        RETURN_THROWS();
    }
    uint16_t swc = sw((uint16_t) color);
    for (size_t i = 0; i < band_px; i++) {
        band[i] = swc;
    }
    for (int y = 0; y < s->height; y += FLUSH_BAND) {
        int h = (y + FLUSH_BAND <= s->height) ? FLUSH_BAND : (s->height - y);
        esp_lcd_panel_draw_bitmap(s->panel, 0, y, s->width, y + h, band);
    }
    free(band);
}

PHP_METHOD(St77916, rgb)
{
    zend_long r, g, b;
    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_LONG(r) Z_PARAM_LONG(g) Z_PARAM_LONG(b)
    ZEND_PARSE_PARAMETERS_END();
    RETURN_LONG(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static const zend_function_entry st77916_methods[] = {
    PHP_ME(St77916, __construct, arginfo_st_ctor, ZEND_ACC_PUBLIC)
    PHP_ME(St77916, fill,        arginfo_st_fill, ZEND_ACC_PUBLIC)
    PHP_ME(St77916, rgb,         arginfo_st_rgb,  ZEND_ACC_PUBLIC)
    PHP_FE_END
};

const spi_driver_desc_t st77916_driver_desc = {
    .name = "st77916",
    .php_class = "St77916",
    .capability = "Baremetal\\Output\\Display",
    .default_cs = 21,
    .default_hz = 40000000,
    .default_mode = 0,
    .managed_io = true,
    .state_size = sizeof(st_state),
    .methods = st77916_methods,
};
#endif
