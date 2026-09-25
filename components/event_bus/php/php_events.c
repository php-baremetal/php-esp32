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
    if (!bm_queue) {
        bm_queue = xQueueCreate(BM_QUEUE_DEPTH, sizeof(bm_event_msg_t));
        if (!bm_queue) {
            return -1;
        }
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
    if (msg->tag >= bm_source_n) {
        return;
    }
    zval event;
    object_init_ex(&event, bm_source_ce[msg->tag]);   /* device stays null for a source event */
    bm_deliver(&event);
    zval_ptr_dtor(&event);
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
    PHP_FE(every,      arginfo_every)
    PHP_FE(watch_gpio, arginfo_watch_gpio)
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
