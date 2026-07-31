/*
 * Application entry point.
 *
 * Mounts the microSD, brings the Zend engine up on a dedicated FreeRTOS task
 * (large stack: the PHP compiler recurses heavily and zend_bailout() relies on
 * setjmp/longjmp), and runs /sdcard/index.php.
 *
 * Two shapes of script are supported:
 *   - a plain script: it just runs top to bottom.
 *   - a setup()/loop() sketch (Arduino-style): setup() runs once, then loop($tick)
 *     is called repeatedly from C. Keeping the loop in C gives us a place for
 *     memory housekeeping and keeps a fatal error in PHP from taking the board
 *     down (we catch the bailout instead of letting it reach exit()).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"   /* board_mount_storage()/board_unmount_storage(), BOARD_NAME */

#include "php_embed.h"
#include "zend_API.h"
#include "zend_execute.h"
#include "zend_stream.h"
#include "zend_exceptions.h"

static const char *TAG = "php-esp32";

/* 64 KB: with a smaller stack the board resets on trivial scripts.
 * ESP-IDF's xTaskCreate takes the stack size in bytes. */
#define PHP_TASK_STACK_BYTES (64 * 1024)

#define SD_MOUNT_POINT "/sdcard"
#define PHP_SCRIPT     SD_MOUNT_POINT "/index.php"

/*
 * Output sink for the engine: echo, print, printf, var_dump and php_printf() all
 * funnel through here. Write straight to the console and always report the full
 * length. The embed SAPI's default ub_write treats a short write as a dropped
 * connection, which fires php_handle_aborted_connection() -> zend_bailout() ->
 * exit(); on this target exit() then aborts inside newlib.
 */
static size_t esp_ub_write(const char *str, size_t len)
{
    fwrite(str, 1, len, stdout);
    fflush(stdout);
    return len;
}

/* Compile and run a file (handles <?php ... ?>). Defines any functions in it. */
static void run_php_file(const char *path)
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
static void run_setup_loop(void)
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

static void php_task(void *arg)
{
    (void)arg;

    /* Line-buffer stdout/stderr: unbuffered streams push newlib through __sbprintf,
     * which creates a per-call lock that aborts on this target. */
    setvbuf(stdout, NULL, _IOLBF, 256);
    setvbuf(stderr, NULL, _IOLBF, 256);

    /* PHP allocations go to PSRAM (see CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0), which
     * keeps internal RAM free for DMA and FreeRTOS. */
    setenv("USE_ZEND_ALLOC", "0", 1);

    bool have_sd = board_mount_storage(SD_MOUNT_POINT);
    if (have_sd) {
        ESP_LOGI(TAG, "storage mounted at %s", SD_MOUNT_POINT);
    } else {
        ESP_LOGW(TAG, "storage not mounted (no card / wrong format?)");
    }

    php_embed_module.ub_write = esp_ub_write;

    ESP_LOGI(TAG, "php_embed_init()...");
    if (php_embed_init(0, NULL) != SUCCESS) {
        ESP_LOGE(TAG, "php_embed_init failed");
        vTaskDelete(NULL);
        return;
    }

    php_printf("PHP %s on %s\n", PHP_VERSION, BOARD_SOC);

    if (have_sd && access(PHP_SCRIPT, R_OK) == 0) {
        php_printf("--- %s ---\n", PHP_SCRIPT);
        /* Catch a PHP bailout (fatal error / die) so it doesn't reach exit(). */
        zend_try {
            run_php_file(PHP_SCRIPT);   /* runs top-level, defines setup()/loop() */
            run_setup_loop();           /* never returns if loop() is defined */
        } zend_catch {
            ESP_LOGE(TAG, "PHP bailed out (fatal error)");
        } zend_end_try();
        php_printf("--- end ---\n");
    } else {
        php_printf("%s not found; engine check: ", PHP_SCRIPT);
        zend_eval_string("echo 1+1;", NULL, "boot");
        php_printf("\n");
    }

    php_embed_shutdown();

    if (have_sd) {
        board_unmount_storage(SD_MOUNT_POINT);
    }

    ESP_LOGI(TAG, "done -- heap free: %u bytes", (unsigned) esp_get_free_heap_size());
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "starting PHP runtime");
    xTaskCreate(php_task, "php", PHP_TASK_STACK_BYTES, NULL, 5, NULL);
}
