/*
 * The two process-lifetime tables. Allocated once and never torn down: a PHP object is a handle into
 * them, and a destructor never touches the hardware. So `new Bus(...)` / `new Device(...)` on the
 * same key return the same underlying entry -- correct across web-server per-request teardown with no
 * extra work.
 */
#pragma once

#include "i2c_driver_desc.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#define I2C_PORT_AUTO      (-1)
#define I2C_DEFAULT_HZ     (100000u)
#define I2C_SCAN_FIRST     (0x08)
#define I2C_SCAN_LAST      (0x77)

typedef enum {
    I2C_OWNER_SYNC  = 0,   /* PHP owns the wire; a per-bus mutex guards each transaction */
    I2C_OWNER_CORE1 = 1,   /* a core-1 task owns the wire (reserved) */
} i2c_owner_t;

typedef struct i2c_bus {
    bool                    in_use;
    int                     port;
    int                     sda;
    int                     scl;
    i2c_owner_t             owner;
    i2c_master_bus_handle_t handle;
    SemaphoreHandle_t       lock;
    uint32_t                addr_used[4];   /* bitmap of the 128 7-bit addresses */
} i2c_bus_t;

struct i2c_dev {
    bool                     in_use;
    i2c_bus_t               *bus;
    uint16_t                 addr;
    uint32_t                 hz;
    i2c_master_dev_handle_t  handle;
    const i2c_driver_desc_t *driver;   /* NULL for a raw device */
    void                    *state;    /* driver per-instance state (state_size), or NULL */
    void                    *poller;   /* executor_poller_t* once poll() is called, or NULL */
};

/* Reset the tables. Runs once at MINIT; creates nothing on the wire. */
void i2c_registry_init(void);

/* Idempotent lookup-or-create for the bus keyed by (port, sda, scl). Returns the entry, or NULL with
 * *err set. owner is honoured only on first creation. */
i2c_bus_t *i2c_bus_get(int port, int sda, int scl, i2c_owner_t owner, esp_err_t *err);

/* Idempotent lookup-or-create for the device keyed by (bus, addr). On first creation it adds the IDF
 * device, allocates driver->state_size and runs driver->init. Returns the entry, or NULL with *err. */
i2c_dev_t *i2c_dev_get(i2c_bus_t *bus, uint16_t addr, uint32_t hz,
                       const i2c_driver_desc_t *driver, esp_err_t *err);

/* The device mounted at addr on bus, or NULL. Used by scan() to annotate answered addresses. */
i2c_dev_t *i2c_dev_find(i2c_bus_t *bus, uint16_t addr);

void i2c_bus_lock(i2c_bus_t *bus);
void i2c_bus_unlock(i2c_bus_t *bus);

/* Driver-facing transport. Each takes the bus lock for the whole transaction. */
esp_err_t i2c_dev_read(i2c_dev_t *d, uint8_t *buf, size_t n);
esp_err_t i2c_dev_write(i2c_dev_t *d, const uint8_t *buf, size_t n);
esp_err_t i2c_dev_read_reg(i2c_dev_t *d, uint8_t reg, uint8_t *buf, size_t n);
esp_err_t i2c_dev_write_reg(i2c_dev_t *d, uint8_t reg, const uint8_t *buf, size_t n);
esp_err_t i2c_dev_write_reg1(i2c_dev_t *d, uint8_t reg, uint8_t val);
