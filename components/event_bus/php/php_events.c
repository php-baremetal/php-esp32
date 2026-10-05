/*
 * Baremetal\Event + Baremetal\Events -- the PHP facade over the event bus.
 *
 * An event is a PHP object (a subclass of Event). listen() maps an event class to listeners (frozen
 * after setup()); now() delivers inline; dispatch() delivers after the current handler returns.
 * Events originated in PHP and delivered to PHP listeners on the same core need no C slot -- the PHP
 * object is the payload; the C pool and cross-core queue come in only when a C producer emits.
 */
#include "php.h"
#include "event_bus_php.h"
#include "zend_exceptions.h"
#include "zend_interfaces.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_pm.h"

static zend_class_entry *bm_event_ce;
static zend_class_entry *bm_events_ce;

static bool     bm_ready;      /* the tables below are lazily initialised in the request */
static bool     bm_frozen;
static bool     bm_draining;
static HashTable bm_listeners; /* class name (zend_string) -> zend_array of callables */
static zval      bm_pending;   /* array used as a FIFO queue of dispatched events */

static void bm_lazy_init(void)
{
    if (bm_ready) {
        return;
    }
    zend_hash_init(&bm_listeners, 8, NULL, ZVAL_PTR_DTOR, 0);
    array_init(&bm_pending);
    bm_ready = true;
}

void bm_events_freeze(void)
{
    bm_frozen = true;
}

/* Call every listener registered for the event's exact class, stopping if one returns false. */
static void bm_deliver(zval *event)
{
    if (!bm_ready) {
        return;
    }
    zval *list = zend_hash_find(&bm_listeners, Z_OBJCE_P(event)->name);
    if (!list || Z_TYPE_P(list) != IS_ARRAY) {
        return;
    }
    zval *cb;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(list), cb) {
        zval ret;
        ZVAL_UNDEF(&ret);
        if (call_user_function(NULL, NULL, cb, &ret, 1, event) != SUCCESS) {
            break;
        }
        bool stop = (Z_TYPE(ret) == IS_FALSE);
        zval_ptr_dtor(&ret);
        if (stop || EG(exception)) {
            break;
        }
    } ZEND_HASH_FOREACH_END();
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_events_listen, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, event, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, listener, IS_CALLABLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_events_event, 0, 1, IS_VOID, 0)
    ZEND_ARG_OBJ_INFO(0, event, Baremetal\\Event, 0)
ZEND_END_ARG_INFO()

/* Events::listen(string $event, callable $listener): void -- only before the table freezes. */
PHP_METHOD(Events, listen)
{
    zend_string *event;
    zval *cb;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STR(event)
        Z_PARAM_ZVAL(cb)
    ZEND_PARSE_PARAMETERS_END();

    if (bm_frozen) {
        zend_throw_exception(zend_ce_exception,
            "Events::listen() must be called in setup(); the listener table is frozen afterwards", 0);
        RETURN_THROWS();
    }
    if (!zend_is_callable(cb, 0, NULL)) {
        zend_argument_type_error(2, "must be a valid callback");
        RETURN_THROWS();
    }
    bm_lazy_init();

    zval *list = zend_hash_find(&bm_listeners, event);
    if (!list) {
        zval arr;
        array_init(&arr);
        list = zend_hash_add(&bm_listeners, event, &arr);
    }
    Z_TRY_ADDREF_P(cb);
    add_next_index_zval(list, cb);
}

/* Events::now(Event $event): void -- deliver inline, right now. */
PHP_METHOD(Events, now)
{
    zval *event;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(event, bm_event_ce)
    ZEND_PARSE_PARAMETERS_END();
    bm_deliver(event);
}

/* Events::dispatch(Event $event): void -- deliver after the current handler returns (breadth-first;
 * events dispatched from a handler run once it unwinds, not re-entrantly). */
PHP_METHOD(Events, dispatch)
{
    zval *event;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(event, bm_event_ce)
    ZEND_PARSE_PARAMETERS_END();

    bm_lazy_init();
    Z_TRY_ADDREF_P(event);
    add_next_index_zval(&bm_pending, event);
    if (bm_draining) {
        return;
    }
    bm_draining = true;
    while (zend_hash_num_elements(Z_ARRVAL(bm_pending)) > 0) {
        zval batch = bm_pending;         /* take the current queue, start a fresh one */
        array_init(&bm_pending);
        zval *ev;
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL(batch), ev) {
            bm_deliver(ev);
            if (EG(exception)) {
                break;
            }
        } ZEND_HASH_FOREACH_END();
        zval_ptr_dtor(&batch);
    }
    bm_draining = false;
}

static const zend_function_entry events_methods[] = {
    PHP_ME(Events, listen,   arginfo_events_listen, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Events, dispatch, arginfo_events_event,  ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Events, now,      arginfo_events_event,  ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

/* ---- The cross-core reactor path: sources (timer, gpio) push a tag; the reactor delivers. -------- */

#define BM_MAX_SOURCES 16
#define BM_QUEUE_DEPTH 32
#define BM_GPIO_DEBOUNCE_US 200000   /* 200 ms */

static QueueHandle_t     bm_queue;
static zend_class_entry *bm_source_ce[BM_MAX_SOURCES];   /* tag -> event class */
static int               bm_source_n;
static volatile int64_t  bm_gpio_last[BM_MAX_SOURCES];
static void            (*bm_http_handler)(void *handle);  /* set by php_ext_web, called by the reactor */
static void            (*bm_ws_handler)(void *handle);    /* set by php_ext_web, called by the reactor */

void bm_events_ensure_queue(void)
{
    if (!bm_queue) {
        bm_queue = xQueueCreate(BM_QUEUE_DEPTH, sizeof(bm_event_msg_t));
    }
}

/* Resolve an event class name to a tag, allocating one on first use. -1 on failure. */
static int bm_source_tag(zend_string *class_name)
{
    zend_class_entry *ce = zend_lookup_class(class_name);
    if (!ce) {
        return -1;
    }
    for (int i = 0; i < bm_source_n; i++) {
        if (bm_source_ce[i] == ce) {
            return i;
        }
    }
    if (bm_source_n >= BM_MAX_SOURCES) {
        return -1;
    }
    bm_events_ensure_queue();
    if (!bm_queue) {
        return -1;
    }
    bm_source_ce[bm_source_n] = ce;
    return bm_source_n++;
}

static void bm_emit_tag(uint16_t tag)
{
    if (bm_queue) {
        bm_event_msg_t msg = { .tag = tag };
        xQueueSend(bm_queue, &msg, 0);        /* drop when full -- bounded, no block */
    }
}

static void IRAM_ATTR bm_gpio_isr(void *arg)
{
    uint16_t tag = (uint16_t) (uintptr_t) arg;
    int64_t now = esp_timer_get_time();
    if (now - bm_gpio_last[tag] < BM_GPIO_DEBOUNCE_US) {
        return;
    }
    bm_gpio_last[tag] = now;
    if (bm_queue) {
        bm_event_msg_t msg = { .tag = tag };
        BaseType_t hp = pdFALSE;
        xQueueSendFromISR(bm_queue, &msg, &hp);
        if (hp) {
            portYIELD_FROM_ISR();
        }
    }
}

static void bm_timer_cb(void *arg)
{
    bm_emit_tag((uint16_t) (uintptr_t) arg);
}

/* ---- Button input source: a debounced per-pin FSM emitting Pressed/Released/Held/Click/Repeat/Double.
 * A shared sampler timer runs the FSM; button_sampling(false) stops it so the chip can light-sleep. ---- */

#define BM_MAX_BUTTONS 4
#define BM_BTN_TICK_MS 10

enum { BTN_PRESSED, BTN_RELEASED, BTN_HELD, BTN_CLICK, BTN_REPEAT, BTN_DOUBLE, BTN_EVENTS };

typedef struct {
    bool    in_use;
    int     pin;
    bool    active_low;
    int     tag[BTN_EVENTS];      /* -1 when that gesture is not wired to a class */
    int     hold_ms, repeat_ms, double_ms, debounce_ms;
    bool    pressed;              /* debounced level */
    int     stable_ms;           /* how long raw has differed from `pressed` */
    int64_t press_t, last_repeat, last_click;
    bool    held_fired, click_pending;
} bm_button_t;

static bm_button_t        bm_buttons[BM_MAX_BUTTONS];
static int                bm_button_n;
static esp_timer_handle_t bm_btn_timer;
static bool               bm_btn_running;

static void bm_btn_emit(bm_button_t *b, int ev)
{
    if (b->tag[ev] >= 0) {
        bm_emit_tag((uint16_t) b->tag[ev]);
    }
}

static void bm_btn_tick(void *arg)
{
    (void) arg;
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < bm_button_n; i++) {
        bm_button_t *b = &bm_buttons[i];
        if (!b->in_use) {
            continue;
        }
        int lvl = gpio_get_level((gpio_num_t) b->pin);
        bool raw = b->active_low ? (lvl == 0) : (lvl != 0);

        if (raw == b->pressed) {
            b->stable_ms = 0;                                 /* matches current state */
        } else {
            b->stable_ms += BM_BTN_TICK_MS;
            if (b->stable_ms >= b->debounce_ms) {             /* stable long enough -> commit */
                b->pressed = raw;
                b->stable_ms = 0;
                if (raw) {
                    bm_btn_emit(b, BTN_PRESSED);
                    b->press_t = now;
                    b->held_fired = false;
                    b->last_repeat = now;
                } else {
                    bm_btn_emit(b, BTN_RELEASED);
                    if (!b->held_fired && (now - b->press_t) < (int64_t) b->hold_ms * 1000) {
                        if (b->double_ms > 0) {
                            if (b->click_pending && (now - b->last_click) < (int64_t) b->double_ms * 1000) {
                                bm_btn_emit(b, BTN_DOUBLE);
                                b->click_pending = false;
                            } else {
                                b->click_pending = true;
                                b->last_click = now;
                            }
                        } else {
                            bm_btn_emit(b, BTN_CLICK);
                        }
                    }
                }
            }
        }

        if (b->pressed) {
            if (!b->held_fired && (now - b->press_t) >= (int64_t) b->hold_ms * 1000) {
                bm_btn_emit(b, BTN_HELD);
                b->held_fired = true;
                b->last_repeat = now;
            } else if (b->held_fired && b->repeat_ms > 0 &&
                       (now - b->last_repeat) >= (int64_t) b->repeat_ms * 1000) {
                bm_btn_emit(b, BTN_REPEAT);
                b->last_repeat = now;
            }
        }
        if (b->click_pending && (now - b->last_click) >= (int64_t) b->double_ms * 1000) {
            bm_btn_emit(b, BTN_CLICK);                        /* no second click arrived -> single */
            b->click_pending = false;
        }
    }
}

static void bm_btn_start(void)
{
    if (!bm_btn_timer) {
        const esp_timer_create_args_t cfg = { .callback = bm_btn_tick, .name = "buttons" };
        if (esp_timer_create(&cfg, &bm_btn_timer) != ESP_OK) {
            return;
        }
    }
    if (!bm_btn_running) {
        esp_timer_start_periodic(bm_btn_timer, (uint64_t) BM_BTN_TICK_MS * 1000);
        bm_btn_running = true;
    }
}

static void bm_btn_stop(void)
{
    if (bm_btn_timer && bm_btn_running) {
        esp_timer_stop(bm_btn_timer);
        bm_btn_running = false;
    }
}

bool bm_events_receive(bm_event_msg_t *out, uint32_t ms)
{
    if (!bm_queue) {
        return false;
    }
    TickType_t wait = (ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(ms);
    return xQueueReceive(bm_queue, out, wait) == pdTRUE;
}

void bm_events_deliver_msg(const bm_event_msg_t *msg)
{
    if (msg->kind == BM_EVT_HTTP) {
        if (bm_http_handler) {
            bm_http_handler(msg->ptr);
        }
        return;
    }
    if (msg->kind == BM_EVT_WS) {
        if (bm_ws_handler) {
            bm_ws_handler(msg->ptr);
        }
        return;
    }
    if (msg->tag >= bm_source_n) {
        return;
    }
    zval event;
    object_init_ex(&event, bm_source_ce[msg->tag]);
    if (msg->ptr) {                                   /* a C producer's emitter -> $e->device */
        zval zdev;
        ZVAL_OBJ(&zdev, (zend_object *) msg->ptr);
        zend_update_property(bm_source_ce[msg->tag], Z_OBJ(event), "device", sizeof("device") - 1, &zdev);
    }
    bm_deliver(&event);
    zval_ptr_dtor(&event);
}

void bm_events_set_http_handler(void (*fn)(void *handle))
{
    bm_http_handler = fn;
}

bool bm_events_post_http(void *handle)
{
    if (!bm_queue) {
        return false;
    }
    bm_event_msg_t msg = { .tag = 0, .kind = BM_EVT_HTTP, .ptr = handle };
    return xQueueSend(bm_queue, &msg, 0) == pdTRUE;
}

void bm_events_set_ws_handler(void (*fn)(void *handle))
{
    bm_ws_handler = fn;
}

bool bm_events_post_ws(void *handle, uint32_t wait_ms)
{
    if (!bm_queue) {
        return false;
    }
    bm_event_msg_t msg = { .tag = 0, .kind = BM_EVT_WS, .ptr = handle };
    TickType_t wait = (wait_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(wait_ms);
    return xQueueSend(bm_queue, &msg, wait) == pdTRUE;   /* blocks: backpressure on the httpd task */
}

void bm_events_deliver_object(zval *event)
{
    bm_deliver(event);
}

int bm_events_deliver_answering(zval *event, zval *ret_out)
{
    ZVAL_UNDEF(ret_out);
    if (!bm_ready) {
        return 0;
    }
    zval *list = zend_hash_find(&bm_listeners, Z_OBJCE_P(event)->name);
    if (!list || Z_TYPE_P(list) != IS_ARRAY) {
        return 0;
    }
    zval *cb;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(list), cb) {
        zval r;
        ZVAL_UNDEF(&r);
        if (call_user_function(NULL, NULL, cb, &r, 1, event) != SUCCESS) {
            zval_ptr_dtor(&r);
            break;
        }
        if (EG(exception)) {
            zval_ptr_dtor(&r);
            return -1;
        }
        if (Z_TYPE(r) == IS_OBJECT) {          /* the first Response wins */
            ZVAL_COPY_VALUE(ret_out, &r);      /* transfer ownership to the caller */
            return 1;
        }
        zval_ptr_dtor(&r);
    } ZEND_HASH_FOREACH_END();
    return 0;
}

zend_class_entry *bm_events_base_ce(void)
{
    return bm_event_ce;
}

bool bm_events_source_tag(zend_string *class_name, uint16_t *out_tag)
{
    int tag = bm_source_tag(class_name);
    if (tag < 0) {
        return false;
    }
    *out_tag = (uint16_t) tag;
    return true;
}

void bm_events_emit(uint16_t tag, void *device)
{
    if (bm_queue) {
        bm_event_msg_t msg = { .tag = tag, .kind = BM_EVT_SOURCE, .ptr = device };
        xQueueSend(bm_queue, &msg, 0);        /* drop when full -- bounded, no block */
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_every, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, ms, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, event, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_watch_gpio, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, pin, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, event, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* every(int $ms, string $event): void -- emit $event every $ms, delivered by the event-driven loop. */
PHP_FUNCTION(every)
{
    zend_long ms;
    zend_string *event;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(ms)
        Z_PARAM_STR(event)
    ZEND_PARSE_PARAMETERS_END();
    if (ms <= 0) {
        zend_argument_value_error(1, "must be greater than 0");
        RETURN_THROWS();
    }
    int tag = bm_source_tag(event);
    if (tag < 0) {
        zend_throw_exception_ex(zend_ce_exception, 0, "every(): unknown class '%s' or too many sources",
                                ZSTR_VAL(event));
        RETURN_THROWS();
    }
    const esp_timer_create_args_t cfg = {
        .callback = bm_timer_cb,
        .arg = (void *) (uintptr_t) tag,
        .name = "every",
    };
    esp_timer_handle_t h;
    if (esp_timer_create(&cfg, &h) != ESP_OK || esp_timer_start_periodic(h, (uint64_t) ms * 1000) != ESP_OK) {
        zend_throw_exception(zend_ce_exception, "every(): cannot start the timer", 0);
        RETURN_THROWS();
    }
}

/* watch_gpio(int $pin, string $event): void -- emit $event on a falling edge (debounced). */
PHP_FUNCTION(watch_gpio)
{
    zend_long pin;
    zend_string *event;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(pin)
        Z_PARAM_STR(event)
    ZEND_PARSE_PARAMETERS_END();
    int tag = bm_source_tag(event);
    if (tag < 0) {
        zend_throw_exception_ex(zend_ce_exception, 0,
                                "watch_gpio(): unknown class '%s' or too many sources", ZSTR_VAL(event));
        RETURN_THROWS();
    }
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io);
    static bool isr_service;
    if (!isr_service) {
        gpio_install_isr_service(0);
        isr_service = true;
    }
    gpio_isr_handler_add((gpio_num_t) pin, bm_gpio_isr, (void *) (uintptr_t) tag);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_watch_button, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, pin, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, on, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, opts, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_button_sampling, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, on, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

/* watch_button(int $pin, array $on, array $opts = []): void
 *   $on   : gesture => event-class, keys among pressed/released/held/click/repeat/double
 *   $opts : holdMs (800), repeatMs (0=off), doubleMs (0=off), debounceMs (30), activeLow (true) */
PHP_FUNCTION(watch_button)
{
    zend_long pin;
    zval *on, *opts = NULL;
    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_LONG(pin)
        Z_PARAM_ARRAY(on)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(opts)
    ZEND_PARSE_PARAMETERS_END();

    if (bm_button_n >= BM_MAX_BUTTONS) {
        zend_throw_exception(zend_ce_exception, "watch_button(): too many buttons", 0);
        RETURN_THROWS();
    }
    bm_button_t *b = &bm_buttons[bm_button_n];
    memset(b, 0, sizeof(*b));
    b->pin = (int) pin;
    b->active_low = true;
    b->hold_ms = 800;
    b->debounce_ms = 30;
    for (int i = 0; i < BTN_EVENTS; i++) {
        b->tag[i] = -1;
    }

    static const struct { const char *k; size_t n; int ev; } gestures[] = {
        { "pressed", sizeof("pressed") - 1, BTN_PRESSED },
        { "released", sizeof("released") - 1, BTN_RELEASED },
        { "held", sizeof("held") - 1, BTN_HELD },
        { "click", sizeof("click") - 1, BTN_CLICK },
        { "repeat", sizeof("repeat") - 1, BTN_REPEAT },
        { "double", sizeof("double") - 1, BTN_DOUBLE },
    };
    HashTable *ht = Z_ARRVAL_P(on);
    for (size_t g = 0; g < sizeof(gestures) / sizeof(gestures[0]); g++) {
        zval *z = zend_hash_str_find(ht, gestures[g].k, gestures[g].n);
        if (z && Z_TYPE_P(z) == IS_STRING) {
            int tag = bm_source_tag(Z_STR_P(z));
            if (tag < 0) {
                zend_throw_exception_ex(zend_ce_exception, 0,
                    "watch_button(): unknown class '%s' or too many sources", Z_STRVAL_P(z));
                RETURN_THROWS();
            }
            b->tag[gestures[g].ev] = tag;
        }
    }

    if (opts) {
        HashTable *o = Z_ARRVAL_P(opts);
        zval *z;
        if ((z = zend_hash_str_find(o, "holdMs", sizeof("holdMs") - 1))) b->hold_ms = (int) zval_get_long(z);
        if ((z = zend_hash_str_find(o, "repeatMs", sizeof("repeatMs") - 1))) b->repeat_ms = (int) zval_get_long(z);
        if ((z = zend_hash_str_find(o, "doubleMs", sizeof("doubleMs") - 1))) b->double_ms = (int) zval_get_long(z);
        if ((z = zend_hash_str_find(o, "debounceMs", sizeof("debounceMs") - 1))) b->debounce_ms = (int) zval_get_long(z);
        if ((z = zend_hash_str_find(o, "activeLow", sizeof("activeLow") - 1))) b->active_low = zend_is_true(z);
    }

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = b->active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = b->active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    b->in_use = true;
    bm_button_n++;
    bm_btn_start();
}

/* button_sampling(bool $on): void -- pause/resume the button sampler (pause before a light sleep). */
PHP_FUNCTION(button_sampling)
{
    zend_bool on;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(on)
    ZEND_PARSE_PARAMETERS_END();
    if (on) {
        bm_btn_start();
    } else {
        bm_btn_stop();
    }
}

/* ---- power_save: a no-light-sleep lock PHP can hold, + the build flag others check. ------------- */

static esp_pm_lock_handle_t bm_pm_lock;
static int                  bm_pm_held;
static bool                 bm_pm_tried;
static bool                 bm_power_save;

static void bm_pm_ensure(void)
{
    if (!bm_pm_tried) {
        bm_pm_tried = true;
        /* fails with ESP_ERR_NOT_SUPPORTED when PM is off (power_save not set) -> lock stays NULL,
         * so power_hold()/power_release() become no-ops. */
        esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "php_hold", &bm_pm_lock);
    }
}

void bm_events_power_autorelease(void)
{
    while (bm_pm_lock && bm_pm_held > 0) {
        esp_pm_lock_release(bm_pm_lock);
        bm_pm_held--;
    }
}

void bm_events_set_power_save(bool on) { bm_power_save = on; }
bool bm_events_power_save(void)        { return bm_power_save; }

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_power_void, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* power_hold(): forbid light sleep until power_release() or the end of the current event handler. */
PHP_FUNCTION(power_hold)
{
    ZEND_PARSE_PARAMETERS_NONE();
    bm_pm_ensure();
    if (bm_pm_lock) {
        esp_pm_lock_acquire(bm_pm_lock);
        bm_pm_held++;
    }
}

/* power_release(): drop one power_hold(). */
PHP_FUNCTION(power_release)
{
    ZEND_PARSE_PARAMETERS_NONE();
    if (bm_pm_lock && bm_pm_held > 0) {
        esp_pm_lock_release(bm_pm_lock);
        bm_pm_held--;
    }
}

PHP_MINIT_FUNCTION(events)
{
    zend_class_entry ce;

    /* Baremetal\Event -- the base class; a subclass is an event. `device` is its emitter (null for a
     * PHP-originated event; a C producer sets it). */
    INIT_NS_CLASS_ENTRY(ce, "Baremetal", "Event", NULL);
    bm_event_ce = zend_register_internal_class(&ce);
    zend_declare_property_null(bm_event_ce, "device", sizeof("device") - 1, ZEND_ACC_PUBLIC);

    INIT_NS_CLASS_ENTRY(ce, "Baremetal", "Events", events_methods);
    bm_events_ce = zend_register_internal_class(&ce);

    bm_ready = false;
    bm_frozen = false;
    bm_draining = false;
    return SUCCESS;
}

static const zend_function_entry events_functions[] = {
    PHP_FE(every,           arginfo_every)
    PHP_FE(watch_gpio,      arginfo_watch_gpio)
    PHP_FE(watch_button,    arginfo_watch_button)
    PHP_FE(button_sampling, arginfo_button_sampling)
    PHP_FE(power_hold,      arginfo_power_void)
    PHP_FE(power_release,   arginfo_power_void)
    PHP_FE_END
};

zend_module_entry events_module_entry = {
    STANDARD_MODULE_HEADER,
    "events",
    events_functions,
    PHP_MINIT(events),
    NULL,   /* MSHUTDOWN */
    NULL,   /* RINIT */
    NULL,   /* RSHUTDOWN */
    NULL,   /* MINFO */
    "1.0",
    STANDARD_MODULE_PROPERTIES
};
