/*
 * The single core-1 executor task. Today it runs device pollers: it samples a device at a fixed rate
 * on core 1 and drops each raw sample into a ring buffer, so PHP on core 0 reads snapshots without
 * touching the bus and without its sample rate depending on how long a handler takes. The task is
 * created lazily on the first poller, so a firmware that never polls pays nothing.
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

/* Copy the most recent sample into out (sample_size bytes). Returns false if none has been taken yet. */
bool executor_poll_latest(executor_poller_t *p, void *out);

/* Copy up to max_samples unread samples into out (contiguous, sample_size bytes each), oldest first,
 * and mark them read. Returns how many were copied. */
size_t executor_poll_drain(executor_poller_t *p, void *out, size_t max_samples);
