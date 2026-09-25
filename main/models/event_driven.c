/* event_driven.c -- the event-driven execution model: run the script top-level (it registers
 * listeners and starts sources), freeze the listener table, then block on the event queue and deliver
 * each event. init-loop without the loop (§6.1): the reactor sleeps between events -- no busy-wait. */
#include <stdio.h>
#include <stdint.h>

#include "esp_log.h"
#include "php_embed.h"
#include "zend_API.h"
#include "zend_exceptions.h"   /* zend_clear_exception */

#include "app.h"             /* g_entry_script */
#include "php_task.h"        /* run_php_file */
#include "event_bus_php.h"   /* bm_events_freeze / receive / deliver_msg */
#include "event_driven.h"

static const char *TAG = "php-esp32";

void run_event_driven(void)
{
    const char *script = g_entry_script;
    printf("--- %s (event-driven) ---\n", script);
    fflush(stdout);

    zend_try {
        run_php_file(script);   /* top-level: Events::listen(...), every(...), watch_gpio(...) */
    } zend_catch {
        ESP_LOGE(TAG, "PHP bailed out during setup (fatal error)");
        printf("--- end ---\n");
        fflush(stdout);
        return;
    } zend_end_try();

    bm_events_freeze();
    ESP_LOGI(TAG, "entering event loop");

    uint32_t n = 0;
    for (;;) {
        bm_event_msg_t msg;
        if (!bm_events_receive(&msg, UINT32_MAX)) {   /* blocks: the idle task runs between events */
            continue;
        }
        zend_try {
            bm_events_deliver_msg(&msg);
            if (EG(exception)) {
                zend_clear_exception();
                ESP_LOGW(TAG, "uncaught exception in an event handler");
            }
        } zend_catch {
            ESP_LOGE(TAG, "PHP bailed out in an event handler");
        } zend_end_try();

        if ((++n & 0xFF) == 0) {
            gc_collect_cycles();
        }
    }
}
