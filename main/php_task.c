/* php_task.c -- the PHP reactor task and its primitives. php_task() is the task body: it lets boot.c
 * bring the runtime up, hands off to the model runner the project type selected (or the engine-check
 * fallback when there's no script), then tears the runtime down. The primitives it and the runners
 * lean on -- compile+run a PHP file, and the Arduino-style setup()/loop() driver with per-call
 * exception handling + periodic GC -- live here too. */
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"    /* esp_get_free_heap_size */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "php_embed.h"
#include "zend_API.h"
#include "zend_execute.h"  /* zend_eval_string (the no-script engine check) */
#include "zend_stream.h"
#include "zend_exceptions.h"

#include "boot.h"          /* boot_php_runtime() / boot_php_shutdown(), the pinning defines */
#include "event_bus_php.h" /* bm_events_freeze() -- close the listener table after setup() */
#include "app.h"           /* g_entry_script -- did boot find a script? */
#include "model_runner.h"  /* model_runner_current() -- the project type's execution model */
#include "php_task.h"

static const char *TAG = "php-esp32";

/*
 * The PHP reactor task. Created (pinned) by app_main() in boot.c. Line-buffers the console, brings
 * the runtime up via boot.c, then runs the selected model (init-loop / web-server / later
 * event-driven) -- most of which never return. With no script it runs a one-shot engine check so the
 * console still shows the engine is alive.
 */
void php_task(void *arg)
{
    (void)arg;

    /* Line-buffer stdout/stderr: unbuffered streams push newlib through __sbprintf, which creates a
     * per-call lock that aborts on this target. Must happen before any output. */
    setvbuf(stdout, NULL, _IOLBF, 256);
    setvbuf(stderr, NULL, _IOLBF, 256);

    /* Report the core we actually landed on -- proves the pinning took (see PHP_TASK_CORE). */
    ESP_LOGI(TAG, "php_task pinned to core %d", xPortGetCoreID());

    if (!boot_php_runtime()) {   /* mounts, network, SAPI hooks, .env, engine, extensions */
        vTaskDelete(NULL);
        return;
    }

    if (g_entry_script) {
        /* Hand off to the model runner the project type selected. The choice lives in one place --
         * model_runner.c -- so this path has no per-model #ifdef. Most runners never return. */
        model_runner_current()->run();
    } else {
        printf("no index.php (embedded or microSD); engine check: ");
        fflush(stdout);
        zend_eval_string("echo 1+1;", NULL, "boot");
        printf("\n");
        fflush(stdout);
    }

    boot_php_shutdown();
    vTaskDelete(NULL);
}

/* Compile and run a file (handles <?php ... ?>). Defines any functions in it. */
void run_php_file(const char *path)
{
    zend_file_handle file_handle;
    zend_stream_init_filename(&file_handle, path);
    if (!php_execute_script(&file_handle)) {
        ESP_LOGE(TAG, "php_execute_script(%s) failed", path);
    }
    zend_destroy_file_handle(&file_handle);
}

static zend_function *find_php_function(const char *name)
{
    return zend_hash_str_find_ptr(EG(function_table), name, strlen(name));
}

/*
 * If the loaded script defined loop(), drive it Arduino-style: setup() once, then
 * loop($tick) forever. delay() inside PHP maps to vTaskDelay(), which yields the
 * core so the watchdog stays happy; a loop() that never calls delay() will trip it.
 */
void run_setup_loop(void)
{
    zend_function *fn_loop = find_php_function("loop");
    if (!fn_loop) {
        return;   /* plain script; it already ran */
    }

    zval ret;

    zend_function *fn_setup = find_php_function("setup");
    if (fn_setup) {
        ZVAL_UNDEF(&ret);
        zend_call_known_function(fn_setup, NULL, NULL, &ret, 0, NULL, NULL);
        zval_ptr_dtor(&ret);
    }
    bm_events_freeze();   /* the Events listener table is immutable once setup() has run */

    ESP_LOGI(TAG, "entering loop()");
    for (uint32_t tick = 0; ; tick++) {
        zval arg;
        ZVAL_LONG(&arg, tick);
        ZVAL_UNDEF(&ret);
        zend_call_known_function(fn_loop, NULL, NULL, &ret, 1, &arg, NULL);
        zval_ptr_dtor(&ret);

        if (EG(exception)) {
            zend_clear_exception();   /* an uncaught PHP exception: log-and-continue */
            ESP_LOGW(TAG, "uncaught exception in loop() at tick %u", (unsigned) tick);
        }

        /* Refcounting frees most garbage immediately; cycles need a periodic sweep,
         * or a long-running device accumulates them until it dies. Also a good spot
         * to watch that memory isn't creeping up. */
        if ((tick & 0xFF) == 0) {
            gc_collect_cycles();
            ESP_LOGI(TAG, "tick %u -- heap free: %u bytes",
                     (unsigned) tick, (unsigned) esp_get_free_heap_size());
        }
    }
}
