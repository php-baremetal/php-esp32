/*
 * i2c_driver_desc_t -- the contract a device driver fills in. A raw device leaves it NULL; a driver
 * fills the fields it needs and leaves the optional blocks zeroed. The five optional blocks
 * (poll/fifo/block/irq/power) let one descriptor serve every execution model without being rewritten.
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* The per-device registry entry (i2c_registry.h); a driver receives it as its handle. */
typedef struct i2c_dev i2c_dev_t;

/* PHP function table; only the pointer is used here, so the engine headers are not pulled in. */
typedef struct _zend_function_entry zend_function_entry;

/* Event class descriptor from the event bus; wired to real events in a later phase. */
typedef struct evt_class_desc evt_class_desc_t;

typedef struct i2c_driver_desc {
    const char *name;              /* manifest key, e.g. "mpu6050" */
    const char *php_class;         /* registered class, e.g. "Mpu6050"; extends I2cDevice */
    const char *capability;        /* interface FQN this driver implements, or NULL */

    uint8_t  default_addrs[4];
    uint8_t  n_default_addrs;
    uint32_t default_hz;

    size_t   state_size;           /* per-instance state, allocated in the device registry */
    esp_err_t (*init)(i2c_dev_t *d);
    void      (*deinit)(i2c_dev_t *d);

    const zend_function_entry *methods;

    /* active polling (init-loop, web-server) */
    struct {
        esp_err_t (*sample)(i2c_dev_t *d, void *out);
        size_t   sample_size;
        uint32_t max_hz;
    } poll;

    /* hardware FIFO + interrupt (event-driven) */
    struct {
        bool      supported;
        esp_err_t (*configure)(i2c_dev_t *d, uint32_t hz, uint16_t watermark);
        esp_err_t (*drain)(i2c_dev_t *d, void *out, size_t *n);
        uint16_t  max_watermark;
    } fifo;

    /* block transfers (framebuffer): asynchronous flush */
    struct {
        bool      supported;
        size_t    buffer_size;
        uint8_t   n_buffers;
        esp_err_t (*transmit)(i2c_dev_t *d, const void *buf, size_t len);
    } block;

    /* interrupt-driven service: the driver owns an INT line, serviced on core 1 */
    struct {
        bool      supported;
        esp_err_t (*service)(i2c_dev_t *d);
    } irq;

    /* sleep behaviour */
    struct {
        bool      survives_sleep;
        esp_err_t (*prepare_sleep)(i2c_dev_t *d);
        esp_err_t (*resume)(i2c_dev_t *d);
    } power;

    /* the event classes this driver emits (tag -> class), registered at MINIT */
    const evt_class_desc_t *events;
    size_t n_events;
} i2c_driver_desc_t;

/* The build generates this table from the selected driver list (see CMakeLists and the manifest).
 * Empty when no drivers are selected. */
extern const i2c_driver_desc_t *const i2c_drivers[];
extern const size_t i2c_driver_count;
