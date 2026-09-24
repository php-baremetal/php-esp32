/*
 * main.c -- app-level shared state for the firmware.
 *
 * The boot sequence itself lives in boot.c (app_main + the runtime bootstrap) and php_task.c (the
 * reactor task + its primitives); the execution models live under models/ (init_loop, web_server,
 * selected by model_runner). What remains here is the small pool of globals those files share via
 * app.h: the board IP, the resolved entry/source published to the model runner, the php-esp32
 * identity strings, and the one helper that copies them into a script's $_SERVER.
 */
#include "zend_API.h"        /* zval */
#include "php_variables.h"   /* php_register_variable */

#include "app.h"

/* The board's own IP once the link is up -- used for $_SERVER['SERVER_ADDR'] in the web-server
 * model. Set in boot.c when the network comes up; empty if there's no network. */
char s_board_ip[16] = "";

/* The resolved entry script and its source mount, published by boot.c once known so the selected
 * model runner (which takes no args) can read them. NULL until then. */
const char *g_entry_script = NULL;
const char *g_src_dir       = NULL;

/* php-esp32 identity, exposed to PHP in $_SERVER (both execution models) and in phpinfo()'s
 * "PHP Baremetal Infos" table (the php component's info.c reads these globals directly). Deliberately
 * NOT setenv'd, so they stay out of $_ENV and the process environment. Set once at boot (boot.c). */
const char *php_esp32_project     = "";
const char *php_esp32_board       = "";
const char *php_esp32_version     = "";
const char *php_esp32_idf_version = "";

/* Add the four PHP_ESP32_* entries to a $_SERVER track-vars array. Shared with both model runners
 * (init_loop.c / web_server.c) via app.h. */
void register_esp32_server_vars(zval *srv)
{
    php_register_variable("PHP_ESP32_PROJECT",     (char *) php_esp32_project,     srv);
    php_register_variable("PHP_ESP32_BOARD",       (char *) php_esp32_board,       srv);
    php_register_variable("PHP_ESP32_VERSION",     (char *) php_esp32_version,     srv);
    php_register_variable("PHP_ESP32_IDF_VERSION", (char *) php_esp32_idf_version, srv);
}
