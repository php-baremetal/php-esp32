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
#include "esp_partition.h"
#include "esp_vfs_fat.h"
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

/* Two independent sources, mounted together when both are present:
 *   - the microSD at /sdcard: writable data (SQLite, logs, files the script writes).
 *   - the embedded PHP source at /app: a read-only FAT image in internal flash, built
 *     only when the firmware is made for "embedded" storage. index.php runs from here if
 *     present, otherwise from the card -- and an embedded project can still use the card
 *     for its data. */
#define SD_MOUNT_POINT  "/sdcard"
#define SD_SCRIPT       SD_MOUNT_POINT "/index.php"
#define APP_MOUNT_POINT "/app"
#define APP_SCRIPT      APP_MOUNT_POINT "/index.php"

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

/* Mount the optional embedded PHP source: a read-only FAT image in the internal
 * 'storage' partition. Absent on microSD-only firmware (the partition may not exist, or
 * exist but hold no image) -- in which case this returns false and we run from the SD. */
static bool s_app_mounted;

static bool mount_embedded(void)
{
    const esp_partition_t *p = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "storage");
    if (!p) {
        return false;
    }
    esp_vfs_fat_mount_config_t cfg = { .max_files = 5 };
    if (esp_vfs_fat_spiflash_mount_ro(APP_MOUNT_POINT, "storage", &cfg) != ESP_OK) {
        return false;
    }
    s_app_mounted = true;
    return true;
}

#ifdef PHP_PROJECT_WEB_SERVER
#include "freertos/semphr.h"
#include "esp_http_server.h"
#include "php_main.h"        /* php_request_startup / php_request_shutdown */
#include "php_variables.h"   /* php_register_variable, TRACK_VARS_SERVER */
#include "SAPI.h"            /* sapi_module (the live output sink) */

/*
 * The web-server execution model. A C HTTP server (esp_http_server) sits in front and PHP is run
 * fresh for each request -- shared-nothing, the way a script runs behind Apache/nginx. The
 * script's output (echo/print/...) is captured and returned as the response body, and the script
 * gets a minimal $_SERVER (method, URI). Selected at build time with -DPHP_PROJECT_WEB_SERVER=ON
 * (the `web-server` project type); the default build uses the run-script + setup()/loop() model.
 *
 * PHP runs in php_task (which already has the big 64 KB stack the compiler needs), NOT in the
 * httpd task: the httpd handler just parks the request, wakes php_task, and waits. So the httpd
 * task can keep a small stack, and PHP always runs on the stack it was set up with. The httpd
 * server handles one request at a time, so a single shared slot is safe.
 */
static const char *s_ws_script;
static char  *s_ws_out;             /* per-request output buffer (grows as needed) */
static size_t s_ws_len, s_ws_cap;
static httpd_req_t *s_ws_req;        /* the request php_task should serve */
static SemaphoreHandle_t s_ws_req_ready;   /* httpd -> php_task: a request is waiting */
static SemaphoreHandle_t s_ws_resp_ready;  /* php_task -> httpd: the response is ready */

/* ub_write sink for this model (runs in php_task): append output to the response buffer. */
static size_t ws_ub_write(const char *str, size_t len)
{
    if (s_ws_len + len > s_ws_cap) {
        size_t ncap = (s_ws_len + len) * 2 + 1024;
        char *n = realloc(s_ws_out, ncap);
        if (!n) {
            return len;   /* drop output under OOM rather than fail the write */
        }
        s_ws_out = n;
        s_ws_cap = ncap;
    }
    memcpy(s_ws_out + s_ws_len, str, len);
    s_ws_len += len;
    return len;
}

/* Give the script a minimal $_SERVER (method + URI), like a real SAPI would. */
static void ws_set_server_vars(httpd_req_t *req)
{
    /* $_SERVER is a JIT auto-global: force it to materialise before we add to it. */
    zend_is_auto_global_str(ZEND_STRL("_SERVER"));
    zval *srv = &PG(http_globals)[TRACK_VARS_SERVER];
    if (Z_TYPE_P(srv) != IS_ARRAY) {
        return;   /* still not an array; skip rather than risk it */
    }
    const char *method = (req->method == HTTP_POST) ? "POST"
                       : (req->method == HTTP_GET)  ? "GET"
                       : "OTHER";
    php_register_variable("REQUEST_METHOD", method, srv);
    php_register_variable("REQUEST_URI", req->uri, srv);
    php_register_variable("SERVER_SOFTWARE", "php-esp32", srv);
}

/* httpd handler (runs in the httpd task): hand the request to php_task, wait for the response. */
static esp_err_t ws_handle(httpd_req_t *req)
{
    s_ws_req = req;
    xSemaphoreGive(s_ws_req_ready);                    /* wake php_task */
    xSemaphoreTake(s_ws_resp_ready, portMAX_DELAY);    /* wait until it has run the script */

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, s_ws_len ? s_ws_out : "", s_ws_len);
    return ESP_OK;
}

/* Run one PHP request cycle for the parked request (runs in php_task). */
static void ws_serve_one(void)
{
    s_ws_len = 0;
    if (php_request_startup() == SUCCESS) {
        zend_try {
            ws_set_server_vars(s_ws_req);
            run_php_file(s_ws_script);   /* fresh compile+run; output -> s_ws_out */
        } zend_catch {
            /* a PHP fatal: whatever was produced before it is the response */
        } zend_end_try();
        php_request_shutdown(NULL);
    }
}

/* Start the HTTP server, then loop in php_task serving one request at a time. php_embed_init()
 * has already brought the engine up and opened one request; we close that so each HTTP request
 * owns a clean cycle. Never returns. */
static void run_web_server(const char *script)
{
    s_ws_script = script;
    /* Redirect output into our per-request buffer. sapi_startup() copied php_embed_module into
     * the live `sapi_module` at php_embed_init() time, so we set that copy, not php_embed_module. */
    sapi_module.ub_write = ws_ub_write;
    php_request_shutdown(NULL);   /* end the request embed_init opened; module stays up */

    s_ws_req_ready  = xSemaphoreCreateBinary();
    s_ws_resp_ready = xSemaphoreCreateBinary();

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.lru_purge_enable = true;   /* httpd task keeps its small default stack -- no PHP here */

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start: %s", esp_err_to_name(err));
        for (;;) { vTaskDelay(pdMS_TO_TICKS(10000)); }   /* don't fall through to shutdown */
    }
    static const httpd_uri_t any_get = {
        .uri = "/*", .method = HTTP_GET, .handler = ws_handle,
    };
    httpd_register_uri_handler(server, &any_get);
    ESP_LOGI(TAG, "web-server model: serving %s over HTTP on :80", script);

    for (;;) {
        xSemaphoreTake(s_ws_req_ready, portMAX_DELAY);   /* a request arrived */
        ws_serve_one();
        xSemaphoreGive(s_ws_resp_ready);                 /* response is in s_ws_out */
    }
}
#endif /* PHP_PROJECT_WEB_SERVER */

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

#ifdef PHP_STORAGE_MICROSD
    /* microSD -- writable data storage, mounted whenever a card is present. Compiled out when
     * microSD support is off (-DPHP_STORAGE_MICROSD=OFF): a board without a card slot, or an
     * embedded project that didn't opt into the card. */
    bool have_sd = board_mount_storage(SD_MOUNT_POINT);
    if (have_sd) {
        ESP_LOGI(TAG, "microSD mounted at %s", SD_MOUNT_POINT);
    } else {
        ESP_LOGW(TAG, "microSD not mounted (no card / wrong format?)");
    }
#endif
    /* Embedded PHP source (read-only) -- only on firmware built for embedded storage. */
    bool have_app = mount_embedded();
    if (have_app) {
        ESP_LOGI(TAG, "embedded source mounted at %s", APP_MOUNT_POINT);
    }

#ifdef BOARD_HAS_NETWORK
    /* Boards with wired networking bring the link up here and log the address, so a PHP
     * script (e.g. a socket server) has the network ready and you can see where to reach
     * it. Non-fatal: without a cable/lease we just log it and carry on. */
    {
        char ip[16];
        if (board_network_up(ip, sizeof ip)) {
            ESP_LOGI(TAG, "network up -- http://%s/", ip);
        } else {
            ESP_LOGW(TAG, "network: no IP (link down or no DHCP)");
        }
    }
#endif

    /* Run the embedded source if it's there, otherwise the one on the card. */
    const char *script = NULL;
    const char *src_dir = NULL;   /* the mount that source lives on (for OPENSSL_CONF below) */
    if (have_app && access(APP_SCRIPT, R_OK) == 0) {
        script = APP_SCRIPT;
        src_dir = APP_MOUNT_POINT;
    }
#ifdef PHP_STORAGE_MICROSD
    else if (have_sd && access(SD_SCRIPT, R_OK) == 0) {
        script = SD_SCRIPT;
        src_dir = SD_MOUNT_POINT;
    }
#endif

    /* The full openssl build needs an openssl.cnf; point it at one shipped with the source
     * (see docs/openssl.md). The path is PHP_OPENSSL_CONF (set from the project config's
     * [extensions.openssl] config_path, default "openssl.cnf"): an absolute path is used as-is,
     * a relative one is resolved against the source mount. Harmless when openssl isn't built or
     * the file isn't there. */
#ifndef PHP_OPENSSL_CONF
#define PHP_OPENSSL_CONF "openssl.cnf"
#endif
    if (src_dir) {
        static char ossl_conf[128];
        if (PHP_OPENSSL_CONF[0] == '/')
            snprintf(ossl_conf, sizeof ossl_conf, "%s", PHP_OPENSSL_CONF);
        else
            snprintf(ossl_conf, sizeof ossl_conf, "%s/%s", src_dir, PHP_OPENSSL_CONF);
        setenv("OPENSSL_CONF", ossl_conf, 1);
    }

    php_embed_module.ub_write = esp_ub_write;

    ESP_LOGI(TAG, "php_embed_init()...");
    if (php_embed_init(0, NULL) != SUCCESS) {
        ESP_LOGE(TAG, "php_embed_init failed");
        vTaskDelete(NULL);
        return;
    }

    php_printf("PHP %s on %s\n", PHP_VERSION, BOARD_SOC);

    if (script) {
#ifdef PHP_PROJECT_WEB_SERVER
        /* web-server model: hand the script to the HTTP server, which runs it per request.
         * Never returns. */
        run_web_server(script);
#else
        php_printf("--- %s ---\n", script);
        /* Catch a PHP bailout (fatal error / die) so it doesn't reach exit(). */
        zend_try {
            run_php_file(script);   /* runs top-level, defines setup()/loop() */
            run_setup_loop();       /* never returns if loop() is defined */
        } zend_catch {
            ESP_LOGE(TAG, "PHP bailed out (fatal error)");
        } zend_end_try();
        php_printf("--- end ---\n");
#endif
    } else {
        php_printf("no index.php (embedded or microSD); engine check: ");
        zend_eval_string("echo 1+1;", NULL, "boot");
        php_printf("\n");
    }

    php_embed_shutdown();

    if (s_app_mounted) {
        esp_vfs_fat_spiflash_unmount_ro(APP_MOUNT_POINT, "storage");
    }
#ifdef PHP_STORAGE_MICROSD
    if (have_sd) {
        board_unmount_storage(SD_MOUNT_POINT);
    }
#endif

    ESP_LOGI(TAG, "done -- heap free: %u bytes", (unsigned) esp_get_free_heap_size());
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "starting PHP runtime");
    xTaskCreate(php_task, "php", PHP_TASK_STACK_BYTES, NULL, 5, NULL);
}
