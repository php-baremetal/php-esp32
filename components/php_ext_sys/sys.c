/*
 * sys extension: system / runtime helpers for the board.
 *
 *   timing    sys_delay(int $ms): void          sleep, yielding the core (never a busy-wait)
 *             sys_uptime_ms(): int               milliseconds since boot
 *             sys_micros(): int                  microseconds since boot
 *   control   sys_restart(): void                reboot the chip
 *             sys_reset_reason(): string         why it last booted (power-on, panic, watchdog, ...)
 *   identity  sys_chip_model(): string           e.g. "ESP32-S3"
 *             sys_cpu_freq_mhz(): int            configured CPU frequency
 *             sys_mac(): string                  the factory MAC (aa:bb:cc:dd:ee:ff)
 *             sys_idf_version(): string          the ESP-IDF version the firmware was built with
 *   memory    sys_psram_free(): int              free PSRAM, bytes
 *             sys_psram_size(): int              total PSRAM pool, bytes
 *             sys_psram_largest_free(): int      largest contiguous free PSRAM block (fragmentation)
 *             sys_heap_free(): int               free internal RAM, bytes
 *             sys_heap_size(): int               total internal RAM pool, bytes
 *             sys_available(): bool              the extension is compiled in
 *
 * delay() is a plain alias of sys_delay() (the Arduino-style idiom). The unprefixed psram_ and heap_
 * names are kept as deprecated aliases of the sys_ memory functions.
 */
#include "php.h"
#include "php_sys.h"
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_mac.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_sys_delay, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, ms, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_sys_void, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_sys_int, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_sys_string, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_sys_bool, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

/* --- timing --- */

static void sys_delay_impl(zend_long ms)
{
    if (ms < 0) {
        ms = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/* sys_delay(int $ms): void */
PHP_FUNCTION(sys_delay)
{
    zend_long ms;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(ms)
    ZEND_PARSE_PARAMETERS_END();
    sys_delay_impl(ms);
}

/* sys_uptime_ms(): int */
PHP_FUNCTION(sys_uptime_ms)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) (esp_timer_get_time() / 1000));
}

/* sys_micros(): int */
PHP_FUNCTION(sys_micros)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) esp_timer_get_time());
}

/* --- control --- */

/* sys_restart(): void -- does not return. */
PHP_FUNCTION(sys_restart)
{
    ZEND_PARSE_PARAMETERS_NONE();
    esp_restart();
}

/* sys_reset_reason(): string */
PHP_FUNCTION(sys_reset_reason)
{
    ZEND_PARSE_PARAMETERS_NONE();
    const char *r;
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:   r = "poweron";   break;
        case ESP_RST_EXT:       r = "external";  break;
        case ESP_RST_SW:        r = "software";  break;
        case ESP_RST_PANIC:     r = "panic";     break;
        case ESP_RST_INT_WDT:   r = "int_wdt";   break;
        case ESP_RST_TASK_WDT:  r = "task_wdt";  break;
        case ESP_RST_WDT:       r = "wdt";       break;
        case ESP_RST_DEEPSLEEP: r = "deepsleep"; break;
        case ESP_RST_BROWNOUT:  r = "brownout";  break;
        case ESP_RST_SDIO:      r = "sdio";      break;
        default:                r = "unknown";   break;
    }
    RETURN_STRING(r);
}

/* --- identity --- */

/* sys_chip_model(): string */
PHP_FUNCTION(sys_chip_model)
{
    ZEND_PARSE_PARAMETERS_NONE();
    esp_chip_info_t info;
    esp_chip_info(&info);
    const char *m;
    switch (info.model) {
        case CHIP_ESP32:   m = "ESP32";    break;
        case CHIP_ESP32S2: m = "ESP32-S2"; break;
        case CHIP_ESP32S3: m = "ESP32-S3"; break;
        case CHIP_ESP32C3: m = "ESP32-C3"; break;
        case CHIP_ESP32C2: m = "ESP32-C2"; break;
        case CHIP_ESP32C6: m = "ESP32-C6"; break;
        case CHIP_ESP32H2: m = "ESP32-H2"; break;
        case CHIP_ESP32P4: m = "ESP32-P4"; break;
        default:           m = "unknown";  break;
    }
    RETURN_STRING(m);
}

/* sys_cpu_freq_mhz(): int -- the configured CPU frequency. */
PHP_FUNCTION(sys_cpu_freq_mhz)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
}

/* sys_mac(): string -- the factory base MAC as aa:bb:cc:dd:ee:ff. */
PHP_FUNCTION(sys_mac)
{
    ZEND_PARSE_PARAMETERS_NONE();
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[18];
    snprintf(buf, sizeof buf, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    RETURN_STRING(buf);
}

/* sys_idf_version(): string */
PHP_FUNCTION(sys_idf_version)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING(esp_get_idf_version());
}

/* --- memory --- */

/* sys_psram_free(): int */
PHP_FUNCTION(sys_psram_free)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

/* sys_psram_size(): int */
PHP_FUNCTION(sys_psram_size)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
}

/* sys_psram_largest_free(): int */
PHP_FUNCTION(sys_psram_largest_free)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
}

/* sys_heap_free(): int */
PHP_FUNCTION(sys_heap_free)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

/* sys_heap_size(): int */
PHP_FUNCTION(sys_heap_size)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
}

/* sys_available(): bool -- the extension is compiled in. */
PHP_FUNCTION(sys_available)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_TRUE;
}

/* delay(int $ms): void -- Arduino-style alias of sys_delay() (see the function table). */

/* Deprecated aliases: the old unprefixed memory names delegate to the sys_* handlers. Registered
 * with PHP_DEP_FE below, so the engine emits E_DEPRECATED on use. */
PHP_FUNCTION(psram_free)         { ZEND_PARSE_PARAMETERS_NONE(); RETURN_LONG((zend_long) heap_caps_get_free_size(MALLOC_CAP_SPIRAM)); }
PHP_FUNCTION(psram_size)         { ZEND_PARSE_PARAMETERS_NONE(); RETURN_LONG((zend_long) heap_caps_get_total_size(MALLOC_CAP_SPIRAM)); }
PHP_FUNCTION(psram_largest_free) { ZEND_PARSE_PARAMETERS_NONE(); RETURN_LONG((zend_long) heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)); }
PHP_FUNCTION(heap_free)          { ZEND_PARSE_PARAMETERS_NONE(); RETURN_LONG((zend_long) heap_caps_get_free_size(MALLOC_CAP_INTERNAL)); }
PHP_FUNCTION(heap_size)          { ZEND_PARSE_PARAMETERS_NONE(); RETURN_LONG((zend_long) heap_caps_get_total_size(MALLOC_CAP_INTERNAL)); }

static const zend_function_entry sys_functions[] = {
    PHP_FE(sys_delay,             arginfo_sys_delay)
    PHP_FE(sys_uptime_ms,         arginfo_sys_int)
    PHP_FE(sys_micros,            arginfo_sys_int)
    PHP_FE(sys_restart,           arginfo_sys_void)
    PHP_FE(sys_reset_reason,      arginfo_sys_string)
    PHP_FE(sys_chip_model,        arginfo_sys_string)
    PHP_FE(sys_cpu_freq_mhz,      arginfo_sys_int)
    PHP_FE(sys_mac,               arginfo_sys_string)
    PHP_FE(sys_idf_version,       arginfo_sys_string)
    PHP_FE(sys_psram_free,        arginfo_sys_int)
    PHP_FE(sys_psram_size,        arginfo_sys_int)
    PHP_FE(sys_psram_largest_free, arginfo_sys_int)
    PHP_FE(sys_heap_free,         arginfo_sys_int)
    PHP_FE(sys_heap_size,         arginfo_sys_int)
    PHP_FE(sys_available,         arginfo_sys_bool)
    PHP_FALIAS(delay, sys_delay,  arginfo_sys_delay)   /* Arduino-style alias of sys_delay() */
    /* deprecated aliases (use the sys_* names) */
    PHP_DEP_FE(psram_free,         arginfo_sys_int)
    PHP_DEP_FE(psram_size,         arginfo_sys_int)
    PHP_DEP_FE(psram_largest_free, arginfo_sys_int)
    PHP_DEP_FE(heap_free,          arginfo_sys_int)
    PHP_DEP_FE(heap_size,          arginfo_sys_int)
    PHP_FE_END
};

zend_module_entry sys_module_entry = {
    STANDARD_MODULE_HEADER,
    "sys",
    sys_functions,
    NULL,   /* MINIT */
    NULL,   /* MSHUTDOWN */
    NULL,   /* RINIT */
    NULL,   /* RSHUTDOWN */
    NULL,   /* MINFO */
    "1.0",
    STANDARD_MODULE_PROPERTIES
};
