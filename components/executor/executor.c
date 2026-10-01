#include "executor.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

#define EXECUTOR_CORE   1
#define EXECUTOR_STACK  8192
#define EXECUTOR_PRIO   4
#define MAX_POLLERS     8
#define MAX_SAMPLE      64
#define MAX_DEPTH       256
#define INTENT_QUEUE_DEPTH  16

/* A unit of work for the executor. fn==NULL is a wake no-op; done is given after fn runs; result gets
 * fn's return. */
typedef struct {
    esp_err_t (*fn)(void *arg);
    void             *arg;
    SemaphoreHandle_t done;
    esp_err_t        *result;
} executor_intent_t;

struct executor_poller {
    bool      active;
    esp_err_t (*sample)(void *dev, void *out);
    void     *dev;
    size_t    sample_size;
    uint32_t  period_ms;
    uint32_t  next_due;      /* ms */
    uint8_t  *ring;
    size_t    depth;
    uint32_t  write_seq;     /* total samples written */
    uint32_t  read_seq;      /* total consumed by drain */
    void     (*notify)(void *arg);   /* called after each new sample (core 1), or NULL */
    void     *notify_arg;
};

static executor_poller_t s_pollers[MAX_POLLERS];
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static QueueHandle_t s_intent_q;
static TaskHandle_t  s_task;
static volatile int  s_poller_count;

static uint32_t now_ms(void)
{
    return (uint32_t) (esp_timer_get_time() / 1000);
}

static void service_pollers(void)
{
    uint32_t now = now_ms();
    for (int i = 0; i < MAX_POLLERS; i++) {
        executor_poller_t *p = &s_pollers[i];
        if (!p->active || (int32_t) (now - p->next_due) < 0) {
            continue;
        }
        uint8_t tmp[MAX_SAMPLE];
        if (p->sample(p->dev, tmp) == ESP_OK) {          /* bus read -- outside the spinlock */
            portENTER_CRITICAL(&s_mux);
            memcpy(p->ring + (p->write_seq % p->depth) * p->sample_size, tmp, p->sample_size);
            p->write_seq++;
            portEXIT_CRITICAL(&s_mux);
            if (p->notify) {
                p->notify(p->notify_arg);                /* post the cross-core event -- no lock held */
            }
        }
        p->next_due = now + p->period_ms;
    }
}

static void executor_task(void *arg)
{
    (void) arg;
    for (;;) {
        /* One-tick wait while polling (keeps cadence, feeds the watchdog); block until work otherwise. */
        TickType_t wait = s_poller_count ? 1 : portMAX_DELAY;
        executor_intent_t it;
        if (xQueueReceive(s_intent_q, &it, wait) == pdTRUE && it.fn) {
            esp_err_t r = it.fn(it.arg);                 /* sole owner -- no lock */
            if (it.result) {
                *it.result = r;
            }
            if (it.done) {
                xSemaphoreGive(it.done);
            }
        }
        service_pollers();
    }
}

bool executor_on_task(void)
{
    return s_task && xTaskGetCurrentTaskHandle() == s_task;
}

/* Create the queue and task on first use (callers run on the PHP task, so no concurrent re-entry). */
static bool executor_ensure_started(void)
{
    if (s_task) {
        return true;
    }
    if (!s_intent_q) {
        s_intent_q = xQueueCreate(INTENT_QUEUE_DEPTH, sizeof(executor_intent_t));
        if (!s_intent_q) {
            return false;
        }
    }
    if (xTaskCreatePinnedToCore(executor_task, "executor", EXECUTOR_STACK, NULL,
                                EXECUTOR_PRIO, &s_task, EXECUTOR_CORE) != pdPASS) {
        s_task = NULL;
        return false;
    }
    return true;
}

esp_err_t executor_run_sync(esp_err_t (*fn)(void *arg), void *arg, uint32_t enqueue_timeout_ms)
{
    if (executor_on_task()) {
        return fn ? fn(arg) : ESP_OK;                    /* already sole owner -- run inline */
    }
    if (!executor_ensure_started()) {
        return ESP_ERR_INVALID_STATE;
    }

    StaticSemaphore_t sbuf;
    SemaphoreHandle_t done = xSemaphoreCreateBinaryStatic(&sbuf);
    esp_err_t result = ESP_FAIL;
    executor_intent_t it = { .fn = fn, .arg = arg, .done = done, .result = &result };

    if (xQueueSend(s_intent_q, &it, pdMS_TO_TICKS(enqueue_timeout_ms)) != pdTRUE) {
        vSemaphoreDelete(done);
        return ESP_ERR_TIMEOUT;                          /* full queue -- backpressure */
    }
    xSemaphoreTake(done, portMAX_DELAY);
    vSemaphoreDelete(done);
    return result;
}

executor_poller_t *executor_poll(esp_err_t (*sample)(void *dev, void *out),
                                 void *dev, size_t sample_size, uint32_t hz, size_t depth)
{
    if (!sample || sample_size == 0 || sample_size > MAX_SAMPLE || hz == 0) {
        return NULL;
    }
    if (depth == 0) {
        depth = 64;
    }
    if (depth > MAX_DEPTH) {
        depth = MAX_DEPTH;
    }
    uint32_t period = 1000u / hz;
    if (period == 0) {
        period = 1;
    }

    /* idempotent: same (sample, dev) updates the rate */
    for (int i = 0; i < MAX_POLLERS; i++) {
        executor_poller_t *p = &s_pollers[i];
        if (p->active && p->sample == sample && p->dev == dev) {
            p->period_ms = period;
            return p;
        }
    }

    executor_poller_t *slot = NULL;
    for (int i = 0; i < MAX_POLLERS; i++) {
        if (!s_pollers[i].active) {
            slot = &s_pollers[i];
            break;
        }
    }
    if (!slot) {
        return NULL;
    }
    uint8_t *ring = calloc(depth, sample_size);
    if (!ring) {
        return NULL;
    }
    slot->sample = sample;
    slot->dev = dev;
    slot->sample_size = sample_size;
    slot->period_ms = period;
    slot->next_due = now_ms();
    slot->ring = ring;
    slot->depth = depth;
    slot->write_seq = 0;
    slot->read_seq = 0;
    slot->notify = NULL;
    slot->notify_arg = NULL;
    portENTER_CRITICAL(&s_mux);
    slot->active = true;                 /* published last */
    portEXIT_CRITICAL(&s_mux);
    s_poller_count++;

    executor_ensure_started();
    if (s_intent_q) {
        executor_intent_t wake = { 0 };   /* nudge an idle loop onto the poller cadence */
        xQueueSend(s_intent_q, &wake, 0);
    }
    return slot;
}

void executor_poll_on_sample(executor_poller_t *p, void (*notify)(void *arg), void *arg)
{
    portENTER_CRITICAL(&s_mux);
    p->notify_arg = arg;
    p->notify = notify;      /* published after its arg */
    portEXIT_CRITICAL(&s_mux);
}

bool executor_poll_latest(executor_poller_t *p, void *out)
{
    bool got = false;
    portENTER_CRITICAL(&s_mux);
    if (p->write_seq > 0) {
        uint32_t idx = (p->write_seq - 1) % p->depth;
        memcpy(out, p->ring + idx * p->sample_size, p->sample_size);
        got = true;
    }
    portEXIT_CRITICAL(&s_mux);
    return got;
}

size_t executor_poll_drain(executor_poller_t *p, void *out, size_t max_samples)
{
    size_t n = 0;
    portENTER_CRITICAL(&s_mux);
    uint32_t oldest = (p->write_seq > p->depth) ? (p->write_seq - p->depth) : 0;
    if (p->read_seq < oldest) {
        p->read_seq = oldest;                /* overwritten samples are lost */
    }
    while (p->read_seq < p->write_seq && n < max_samples) {
        uint32_t idx = p->read_seq % p->depth;
        memcpy((uint8_t *) out + n * p->sample_size, p->ring + idx * p->sample_size, p->sample_size);
        p->read_seq++;
        n++;
    }
    portEXIT_CRITICAL(&s_mux);
    return n;
}
