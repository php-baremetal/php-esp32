#pragma once

/* A model runner: one execution model, each living in its own models/<name>.c -- init-loop
 * (run-once + setup()/loop()), web-server (esp_http_server, PHP per request), and later
 * event-driven (block on the event queue). run() takes the reactor to completion; most never
 * return. It takes no arguments -- the entry script and its source mount are read from g_entry_script
 * / g_src_dir (app.h), published by php_task before the hand-off.
 *
 * The build's project type selects exactly one runner (model_runner_current). Adding a model is a
 * new models/<name>.c plus one arm in model_runner.c -- never another #ifdef in the boot path. */
typedef struct {
    void (*run)(void);
} model_runner_t;

/* The runner selected by this build's project type: web-server when PHP_PROJECT_WEB_SERVER is set,
 * otherwise the init-loop. Never NULL. */
const model_runner_t *model_runner_current(void);
