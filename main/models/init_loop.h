#pragma once

/* The init-loop (run-once) execution model, defined in init_loop.c: run the entry script top-level,
 * then drive setup()/loop() if defined. The model_runner run() entry -- takes no arguments,
 * reads g_entry_script (app.h). Selected when PHP_PROJECT_WEB_SERVER is off. */
void run_init_loop(void);
