/* init_loop.c -- the init-loop (run-once) execution model: run the script once at top level, then,
 * if it defined loop(), drive setup()/loop() Arduino-style. Moved out of main.c. */
#include <stdio.h>

#include "esp_log.h"

#include "php_embed.h"
#include "SAPI.h"            /* SG() */
#include "php_variables.h"   /* php_register_variable, TRACK_VARS_SERVER */
#include "zend_API.h"

#include "app.h"             /* register_esp32_server_vars */
#include "php_task.h"        /* run_php_file, run_setup_loop */
#include "init_loop.h"

static const char *TAG = "php-esp32";

static void set_run_once_server_vars(const char *script)
{
    zend_is_auto_global_str(ZEND_STRL("_SERVER"));   /* force the JIT auto-global to materialise */
    zval *srv = &PG(http_globals)[TRACK_VARS_SERVER];
    if (Z_TYPE_P(srv) != IS_ARRAY) {
        return;
    }
    php_register_variable("REQUEST_METHOD", "GET", srv);
    php_register_variable("REQUEST_URI", "/", srv);
    php_register_variable("SCRIPT_NAME", "/index.php", srv);
    php_register_variable("PHP_SELF", "/index.php", srv);
    php_register_variable("SCRIPT_FILENAME", (char *) script, srv);
    php_register_variable("SERVER_PROTOCOL", "HTTP/1.1", srv);
    php_register_variable("SERVER_NAME", "esp32", srv);
    php_register_variable("HTTP_HOST", "esp32", srv);
    php_register_variable("SERVER_PORT", "80", srv);
    php_register_variable("REMOTE_ADDR", "127.0.0.1", srv);
    php_register_variable("SERVER_SOFTWARE", "php-esp32", srv);
    register_esp32_server_vars(srv);
}

void run_init_loop(void)
{
    const char *script = g_entry_script;   /* published by php_task (app.h) */
    printf("--- %s ---\n", script);
    fflush(stdout);
    /* The embed SAPI marks headers as already sent at init (it's a CLI-like, no-HTTP SAPI). In this
     * run-once model there are no HTTP headers, so clear it before the script: otherwise
     * session_start()/setcookie()/header() and the session ini settings all refuse with "headers
     * already sent". Header ops stay no-ops (SG(request_info).no_headers), so this only lifts the
     * false warning. (The web-server model keeps the flag -- it sends real headers.) */
    SG(headers_sent) = 0;
    set_run_once_server_vars(script);   /* a sane GET / $_SERVER for a framework front controller */
    /* Catch a PHP bailout (fatal error / die) so it doesn't reach exit(). */
    zend_try {
        run_php_file(script);   /* runs top-level, defines setup()/loop() */
        run_setup_loop();       /* never returns if loop() is defined */
    } zend_catch {
        ESP_LOGE(TAG, "PHP bailed out (fatal error)");
    } zend_end_try();
    printf("--- end ---\n");
    fflush(stdout);
}
