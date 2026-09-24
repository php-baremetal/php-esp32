#pragma once
#include "zend_API.h"   /* zval */

/* Cross-model shared state, defined in main.c and populated during boot (boot.c):
 *   s_board_ip                 -- the board's IP for $_SERVER['SERVER_ADDR'] (web-server); "" if none.
 *   register_esp32_server_vars -- add the PHP_ESP32_* identity entries to a $_SERVER array (both models). */
extern char s_board_ip[16];
void register_esp32_server_vars(zval *srv);

/* The resolved entry script and its source-mount directory, published by boot_php_runtime() (boot.c)
 * once known and before php_task hands off to the selected model runner. A runner takes no arguments
 * (model_runner_t), so it reads these: the web-server resolves its init script under g_src_dir,
 * the init-loop only needs g_entry_script. Both are NULL until boot sets them (NULL entry => no
 * script found, and php_task runs the engine-check fallback instead of a model). */
extern const char *g_entry_script;
extern const char *g_src_dir;

/* php-esp32 identity strings, set once during boot (boot.c) and read into $_SERVER by
 * register_esp32_server_vars (main.c) and into phpinfo()'s "PHP Baremetal Infos" table by the php
 * component's info.c. Never setenv'd, so they stay out of $_ENV. */
extern const char *php_esp32_project;
extern const char *php_esp32_board;
extern const char *php_esp32_version;
extern const char *php_esp32_idf_version;
