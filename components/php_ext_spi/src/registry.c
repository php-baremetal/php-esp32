#ifdef PHP_SPI_BUILD
#include "spi_registry.h"

#include <string.h>
#include <stdlib.h>

#define MAX_BUSES    2
#define MAX_DEVICES  8
#define BUS_MAX_XFER 32768
#define XFER_TIMEOUT_MS 1000

static spi_bus_t s_buses[MAX_BUSES];
static spi_dev_t s_devs[MAX_DEVICES];

void spi_registry_init(void)
{
    memset(s_buses, 0, sizeof(s_buses));
    memset(s_devs, 0, sizeof(s_devs));
}

spi_bus_t *spi_bus_get(int host, int sclk, int mosi, int miso, int data2, int data3, esp_err_t *err)
{
    if (err) {
        *err = ESP_OK;
    }
    for (int i = 0; i < MAX_BUSES; i++) {
        if (s_buses[i].in_use && s_buses[i].host == host) {
            return &s_buses[i];
        }
    }

    spi_bus_t *slot = NULL;
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

    bool quad = (data2 >= 0 && data3 >= 0);
    spi_bus_config_t cfg = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sclk,
        .quadwp_io_num = quad ? data2 : -1,
        .quadhd_io_num = quad ? data3 : -1,
        .max_transfer_sz = BUS_MAX_XFER,
    };
    esp_err_t e = spi_bus_initialize(host, &cfg, SPI_DMA_CH_AUTO);
    if (e != ESP_OK) {
        if (err) {
            *err = e;
        }
        return NULL;
    }

    slot->in_use = true;
    slot->host = host;
    slot->sclk = sclk;
    slot->mosi = mosi;
    slot->miso = miso;
    slot->data2 = data2;
    slot->data3 = data3;
    slot->quad = quad;
    return slot;
}

static spi_dev_t *dev_find(spi_bus_t *bus, int cs)
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (s_devs[i].in_use && s_devs[i].bus == bus && s_devs[i].cs == cs) {
            return &s_devs[i];
        }
    }
    return NULL;
}

static spi_dev_t *dev_slot(void)
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!s_devs[i].in_use) {
            return &s_devs[i];
        }
    }
    return NULL;
}

spi_dev_t *spi_dev_reserve(spi_bus_t *bus, int cs, const spi_driver_desc_t *driver, esp_err_t *err)
{
    if (err) {
        *err = ESP_OK;
    }
    spi_dev_t *found = dev_find(bus, cs);
    if (found) {
        return found;
    }
    spi_dev_t *slot = dev_slot();
    if (!slot) {
        if (err) {
            *err = ESP_ERR_NO_MEM;
        }
        return NULL;
    }
    void *state = NULL;
    if (driver && driver->state_size) {
        state = calloc(1, driver->state_size);
        if (!state) {
            if (err) {
                *err = ESP_ERR_NO_MEM;
            }
            return NULL;
        }
    }
    slot->in_use = true;
    slot->bus = bus;
    slot->cs = cs;
    slot->handle = NULL;
    slot->driver = driver;
    slot->state = state;
    return slot;
}

spi_dev_t *spi_dev_get(spi_bus_t *bus, int cs, uint32_t hz, uint8_t mode,
                       const spi_driver_desc_t *driver, esp_err_t *err)
{
    if (err) {
        *err = ESP_OK;
    }
    spi_dev_t *found = dev_find(bus, cs);
    if (found) {
        return found;
    }
    spi_dev_t *slot = spi_dev_reserve(bus, cs, driver, err);
    if (!slot) {
        return NULL;
    }

    spi_device_interface_config_t dc = {
        .clock_speed_hz = hz ? hz : SPI_DEFAULT_HZ,
        .mode = mode,
        .spics_io_num = cs,
        .queue_size = 4,
    };
    esp_err_t e = spi_bus_add_device(bus->host, &dc, &slot->handle);
    if (e != ESP_OK) {
        free(slot->state);
        memset(slot, 0, sizeof(*slot));
        if (err) {
            *err = e;
        }
        return NULL;
    }
    slot->hz = dc.clock_speed_hz;
    slot->mode = mode;

    if (driver && driver->init) {
        e = driver->init(slot);
        if (e != ESP_OK) {
            spi_bus_remove_device(slot->handle);
            free(slot->state);
            memset(slot, 0, sizeof(*slot));
            if (err) {
                *err = e;
            }
            return NULL;
        }
    }
    return slot;
}

esp_err_t spi_dev_transfer(spi_dev_t *d, const uint8_t *tx, uint8_t *rx, size_t n)
{
    if (!d->handle) {
        return ESP_ERR_INVALID_STATE;
    }
    spi_transaction_t t = {
        .length = n * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    return spi_device_transmit(d->handle, &t);
}
#endif /* PHP_SPI_BUILD */
