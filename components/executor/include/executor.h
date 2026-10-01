/*
 * The single core-1 executor task: device pollers (sample into a ring) and handed-over work (a CORE1 bus
 * runs its transactions here as sole owner). Created lazily; a firmware that uses neither pays nothing.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct executor_poller executor_poller_t;

/* Register (or update) a poller for a device. The executor calls sample(dev, out) at `hz` on core 1
 * and stores the raw sample (sample_size bytes) into a ring of `depth` slots. sample() must do its own
 * bus locking; it runs on core 1. Idempotent per (sample, dev): a second call updates the rate.
 * Returns the poller handle, or NULL on failure. */
executor_poller_t *executor_poll(esp_err_t (*sample)(void *dev, void *out),
                                 void *dev, size_t sample_size, uint32_t hz, size_t depth);

/* Install a notify called on core 1 right after each new sample is stored (outside the ring lock).
 * It must not block or touch PHP -- a source uses it to post a cross-core message (a queue send). NULL
 * clears it. arg is passed through. */
void executor_poll_on_sample(executor_poller_t *p, void (*notify)(void *arg), void *arg);

/* Copy the most recent sample into out (sample_size bytes). Returns false if none has been taken yet. */
bool executor_poll_latest(executor_poller_t *p, void *out);

/* Copy up to max_samples unread samples into out (contiguous, sample_size bytes each), oldest first,
 * and mark them read. Returns how many were copied. */
size_t executor_poll_drain(executor_poller_t *p, void *out, size_t max_samples);

/* Run fn(arg) on the executor task and block until done, returning its result; run inline if already on
 * the task. Starts the executor lazily. A full queue for enqueue_timeout_ms returns ESP_ERR_TIMEOUT. */
esp_err_t executor_run_sync(esp_err_t (*fn)(void *arg), void *arg, uint32_t enqueue_timeout_ms);

/* True when the caller runs on the executor task. */
bool executor_on_task(void);
