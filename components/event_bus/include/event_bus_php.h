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

/* The cross-core reactor path, used by the event-driven model runner. A source (timer, gpio, the
 * executor, the http server) pushes a message into a FreeRTOS queue; the reactor blocks on it and
 * delivers. `kind` says how to deliver: a source event (materialise `tag`'s class, `ptr` is the
 * emitter zend_object * or NULL) or an HTTP request (`ptr` is an opaque handle passed to the handler
 * registered with bm_events_set_http_handler). */
#define BM_EVT_SOURCE 0
#define BM_EVT_HTTP   1
#define BM_EVT_WS     2
typedef struct { uint16_t tag; uint8_t kind; void *ptr; } bm_event_msg_t;

/* Block up to `ms` (UINT32_MAX = forever) for the next event. Returns false on timeout / no queue. */
bool bm_events_receive(bm_event_msg_t *out, uint32_t ms);

/* Materialise the PHP event object for a received message and deliver it to the listeners. Call only
 * from the reactor (php_task, core 0). */
void bm_events_deliver_msg(const bm_event_msg_t *msg);

/* Release any power_hold() still held after an event handler returned (the reactor calls this after each
 * delivery, so a handler can't leak the no-light-sleep lock). */
void bm_events_power_autorelease(void);

/* Record whether the build enabled power_save (esp_pm). main sets it at startup; other components read
 * it to reject contradictory configs (e.g. a CORE1 I2C bus). */
void bm_events_set_power_save(bool on);
bool bm_events_power_save(void);

/* The Baremetal\Event base class entry, so another extension can register event subclasses. */
struct _zend_class_entry *bm_events_base_ce(void);

/* Map an event class name to a source tag (allocating one, and the queue, on first use). Returns false
 * if the class is unknown or the source table is full. Call from PHP (core 0), before the freeze. */
bool bm_events_source_tag(struct _zend_string *class_name, uint16_t *out_tag);

/* Post a source event for `tag` with `device` (a zend_object *, or NULL) as its emitter. Safe to call
 * from a core-1 task; drops silently when the queue is full (bounded -- backpressure, never a block). */
void bm_events_emit(uint16_t tag, void *device);

/* Ensure the cross-core queue exists (a producer with no source tag -- e.g. the http server -- calls
 * this before posting). The event-driven reactor needs the queue to exist to receive anything. */
void bm_events_ensure_queue(void);

/* Post an HTTP request onto the reactor queue: the reactor calls the registered http handler with
 * `handle`. Returns false if there is no queue or it is full. Safe from the httpd task. */
bool bm_events_post_http(void *handle);

/* Register the function the reactor calls (on core 0) for a BM_EVT_HTTP message. */
void bm_events_set_http_handler(void (*fn)(void *handle));

/* Post an inbound WebSocket message onto the reactor queue: the reactor calls the registered ws handler
 * with `handle`. Blocks up to `wait_ms` (UINT32_MAX = forever) for room -- backpressure from the httpd
 * task. Returns false if there is no queue or the wait expired (caller then frees the handle). */
bool bm_events_post_ws(void *handle, uint32_t wait_ms);

/* Register the function the reactor calls (on core 0) for a BM_EVT_WS message. */
void bm_events_set_ws_handler(void (*fn)(void *handle));

/* Deliver `event` to its class's listeners, fire-and-forget (a listener returning false stops the chain). */
void bm_events_deliver_object(struct _zval_struct *event);

/* Deliver `event` to its listeners, capturing the first that returns an object into `ret_out` (caller
 * dtors it). Returns 1 if answered, 0 if none, -1 if a listener threw. The first Response wins. */
int bm_events_deliver_answering(struct _zval_struct *event, struct _zval_struct *ret_out);
