#include "executor.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

#define EXECUTOR_CORE   1
#define EXECUTOR_STACK  8192
#define EXECUTOR_PRIO   4
#define MAX_POLLERS     8
#define MAX_SAMPLE      64
#define MAX_DEPTH       256

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
};

static executor_poller_t s_pollers[MAX_POLLERS];
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_started;

static uint32_t now_ms(void)
{
    return (uint32_t) (esp_timer_get_time() / 1000);
}

static void executor_task(void *arg)
{
    (void) arg;
    for (;;) {
        uint32_t now = now_ms();
        for (int i = 0; i < MAX_POLLERS; i++) {
            executor_poller_t *p = &s_pollers[i];
            if (!p->active || (int32_t) (now - p->next_due) < 0) {
                continue;
            }
            uint8_t tmp[MAX_SAMPLE];
            if (p->sample(p->dev, tmp) == ESP_OK) {      /* bus read -- outside the spinlock */
                portENTER_CRITICAL(&s_mux);
                memcpy(p->ring + (p->write_seq % p->depth) * p->sample_size, tmp, p->sample_size);
                p->write_seq++;
                portEXIT_CRITICAL(&s_mux);
            }
            p->next_due = now + p->period_ms;
        }
        vTaskDelay(1);   /* yield + feed the idle watchdog */
    }
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
    portENTER_CRITICAL(&s_mux);
    slot->active = true;                 /* published last */
    portEXIT_CRITICAL(&s_mux);

    if (!s_started) {
        s_started = true;
        xTaskCreatePinnedToCore(executor_task, "executor", EXECUTOR_STACK, NULL,
                                EXECUTOR_PRIO, NULL, EXECUTOR_CORE);
    }
    return slot;
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
