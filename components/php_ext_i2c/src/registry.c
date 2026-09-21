#ifdef PHP_I2C_BUILD
#include "i2c_registry.h"

#include <string.h>
#include <stdlib.h>

#define MAX_BUSES    4
#define MAX_DEVICES  32

static i2c_bus_t s_buses[MAX_BUSES];
static i2c_dev_t s_devs[MAX_DEVICES];

void i2c_registry_init(void)
{
    memset(s_buses, 0, sizeof(s_buses));
    memset(s_devs, 0, sizeof(s_devs));
}

static void addr_mark(i2c_bus_t *bus, uint16_t addr)
{
    if (addr < 128) {
        bus->addr_used[addr >> 5] |= (1u << (addr & 31));
    }
}

i2c_bus_t *i2c_bus_get(int port, int sda, int scl, i2c_owner_t owner, esp_err_t *err)
{
    if (err) {
        *err = ESP_OK;
    }

    for (int i = 0; i < MAX_BUSES; i++) {
        i2c_bus_t *b = &s_buses[i];
        if (b->in_use && b->port == port && b->sda == sda && b->scl == scl) {
            return b;
        }
    }

    i2c_bus_t *slot = NULL;
    for (int i = 0; i < MAX_BUSES; i++) {
        if (!s_buses[i].in_use) {
            slot = &s_buses[i];
            break;
        }
    }
    if (!slot) {
        if (err) {
            *err = ESP_ERR_NO_MEM;
        }
        return NULL;
    }

    i2c_master_bus_config_t cfg = {
        .i2c_port = port,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t handle;
    esp_err_t e = i2c_new_master_bus(&cfg, &handle);
    if (e != ESP_OK) {
        if (err) {
            *err = e;
        }
        return NULL;
    }

    SemaphoreHandle_t lock = xSemaphoreCreateMutex();
    if (!lock) {
        i2c_del_master_bus(handle);
        if (err) {
            *err = ESP_ERR_NO_MEM;
        }
        return NULL;
    }

    slot->in_use = true;
    slot->port = port;
    slot->sda = sda;
    slot->scl = scl;
    slot->owner = owner;
    slot->handle = handle;
    slot->lock = lock;
    memset(slot->addr_used, 0, sizeof(slot->addr_used));
    return slot;
}

i2c_dev_t *i2c_dev_find(i2c_bus_t *bus, uint16_t addr)
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        i2c_dev_t *d = &s_devs[i];
        if (d->in_use && d->bus == bus && d->addr == addr) {
            return d;
        }
    }
    return NULL;
}

i2c_dev_t *i2c_dev_get(i2c_bus_t *bus, uint16_t addr, uint32_t hz,
                       const i2c_driver_desc_t *driver, esp_err_t *err)
{
    if (err) {
        *err = ESP_OK;
    }

    i2c_dev_t *found = i2c_dev_find(bus, addr);
    if (found) {
        return found;
    }

    i2c_dev_t *slot = NULL;
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!s_devs[i].in_use) {
            slot = &s_devs[i];
            break;
        }
    }
    if (!slot) {
        if (err) {
            *err = ESP_ERR_NO_MEM;
        }
        return NULL;
    }

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = hz ? hz : I2C_DEFAULT_HZ,
    };
    i2c_master_dev_handle_t handle;
    esp_err_t e = i2c_master_bus_add_device(bus->handle, &cfg, &handle);
    if (e != ESP_OK) {
        if (err) {
            *err = e;
        }
        return NULL;
    }

    void *state = NULL;
    if (driver && driver->state_size) {
        state = calloc(1, driver->state_size);
        if (!state) {
            i2c_master_bus_rm_device(handle);
            if (err) {
                *err = ESP_ERR_NO_MEM;
            }
            return NULL;
        }
    }

    slot->in_use = true;
    slot->bus = bus;
    slot->addr = addr;
    slot->hz = hz ? hz : I2C_DEFAULT_HZ;
    slot->handle = handle;
    slot->driver = driver;
    slot->state = state;

    if (driver && driver->init) {
        e = driver->init(slot);
        if (e != ESP_OK) {
            free(state);
            i2c_master_bus_rm_device(handle);
            memset(slot, 0, sizeof(*slot));
            if (err) {
                *err = e;
            }
            return NULL;
        }
    }

    addr_mark(bus, addr);
    return slot;
}

void i2c_bus_lock(i2c_bus_t *bus)
{
    xSemaphoreTake(bus->lock, portMAX_DELAY);
}

void i2c_bus_unlock(i2c_bus_t *bus)
{
    xSemaphoreGive(bus->lock);
}

#define DEV_TIMEOUT_MS  1000

esp_err_t i2c_dev_read(i2c_dev_t *d, uint8_t *buf, size_t n)
{
    i2c_bus_lock(d->bus);
    esp_err_t e = i2c_master_receive(d->handle, buf, n, DEV_TIMEOUT_MS);
    i2c_bus_unlock(d->bus);
    return e;
}

esp_err_t i2c_dev_write(i2c_dev_t *d, const uint8_t *buf, size_t n)
{
    i2c_bus_lock(d->bus);
    esp_err_t e = i2c_master_transmit(d->handle, buf, n, DEV_TIMEOUT_MS);
    i2c_bus_unlock(d->bus);
    return e;
}

esp_err_t i2c_dev_read_reg(i2c_dev_t *d, uint8_t reg, uint8_t *buf, size_t n)
{
    i2c_bus_lock(d->bus);
    esp_err_t e = i2c_master_transmit_receive(d->handle, &reg, 1, buf, n, DEV_TIMEOUT_MS);
    i2c_bus_unlock(d->bus);
    return e;
}

esp_err_t i2c_dev_write_reg(i2c_dev_t *d, uint8_t reg, const uint8_t *buf, size_t n)
{
    uint8_t stack[1 + 32];
    uint8_t *p = stack;
    if (1 + n > sizeof(stack)) {
        p = malloc(1 + n);
        if (!p) {
            return ESP_ERR_NO_MEM;
        }
    }
    p[0] = reg;
    memcpy(p + 1, buf, n);
    i2c_bus_lock(d->bus);
    esp_err_t e = i2c_master_transmit(d->handle, p, 1 + n, DEV_TIMEOUT_MS);
    i2c_bus_unlock(d->bus);
    if (p != stack) {
        free(p);
    }
    return e;
}

esp_err_t i2c_dev_write_reg1(i2c_dev_t *d, uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    i2c_bus_lock(d->bus);
    esp_err_t e = i2c_master_transmit(d->handle, b, 2, DEV_TIMEOUT_MS);
    i2c_bus_unlock(d->bus);
    return e;
}
#endif /* PHP_I2C_BUILD */
