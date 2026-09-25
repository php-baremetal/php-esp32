/*
 * PHP binding of the event bus: Baremetal\Event (base) and the Baremetal\Events facade
 * (listen/dispatch/now). Kept separate from the host-testable C core (src/).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Freeze the listener table: after this, Events::listen() throws. The runner calls it once the
 * top-level (or setup()) has run (the listener map must not mutate while core 1 / ISRs may read it). */
void bm_events_freeze(void);

/* The cross-core reactor path, used by the event-driven model runner. A source (timer, gpio, later
 * the executor) pushes a tag into a FreeRTOS queue; the reactor blocks on it and delivers. */
typedef struct { uint16_t tag; } bm_event_msg_t;

/* Block up to `ms` (UINT32_MAX = forever) for the next event. Returns false on timeout / no queue. */
bool bm_events_receive(bm_event_msg_t *out, uint32_t ms);

/* Materialise the PHP event object for a received message and deliver it to the listeners. Call only
 * from the reactor (php_task, core 0). */
void bm_events_deliver_msg(const bm_event_msg_t *msg);
