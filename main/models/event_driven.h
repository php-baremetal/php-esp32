#pragma once

/* The event-driven execution model, defined in event_driven.c: run the entry script top-level (which
 * registers listeners and starts sources), freeze the listener table, then block on the event queue
 * and deliver each event to its listeners. The model_runner run() entry (§8.2) -- takes no arguments,
 * reads g_entry_script (app.h). Selected when PHP_PROJECT_EVENT_DRIVEN is set. */
void run_event_driven(void);
