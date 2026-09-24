#pragma once

/* The web-server execution model, defined in web_server.c: an esp_http_server in front of PHP, run
 * fresh per request. The model_runner run() entry -- takes no arguments, reads g_entry_script
 * / g_src_dir (app.h) and resolves its own -DPHP_WEB_INIT init script. Selected when
 * PHP_PROJECT_WEB_SERVER is set. Never returns. */
void run_web_server(void);
