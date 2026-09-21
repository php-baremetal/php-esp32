/*
 * The two process-lifetime tables for the SPI bus, mirroring the I2C registry. Allocated once and
 * never torn down: a PHP object is a handle into them and a destructor never touches the hardware,
 * so a display's framebuffer and panel survive web-server per-request teardown.
 */
#pragma once

#include "spi_driver_desc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#define SPI_DEFAULT_HZ  (1000000u)

typedef struct spi_bus {
    bool               in_use;
    int                host;              /* SPI2_HOST / SPI3_HOST -- the bus key */
    int                sclk, mosi, miso;
    int                data2, data3;      /* QSPI extra lines, -1 when unused */
    bool               quad;
} spi_bus_t;

struct spi_dev {
    bool                    in_use;
    spi_bus_t              *bus;
    int                     cs;
    uint32_t                hz;
    uint8_t                 mode;
    spi_device_handle_t     handle;       /* raw transport handle; NULL for a managed-io driver */
    const spi_driver_desc_t *driver;      /* NULL for a raw device */
    void                   *state;        /* driver per-instance state, or NULL */
};

/* Reset the tables. Runs once at MINIT; creates nothing on the wire. */
void spi_registry_init(void);

/* Idempotent lookup-or-create for the bus keyed by host. Initialises the SPI host on first use. */
spi_bus_t *spi_bus_get(int host, int sclk, int mosi, int miso, int data2, int data3, esp_err_t *err);

/* Idempotent lookup-or-create for a raw device keyed by (bus, cs). Adds the IDF device. */
spi_dev_t *spi_dev_get(spi_bus_t *bus, int cs, uint32_t hz, uint8_t mode,
                       const spi_driver_desc_t *driver, esp_err_t *err);

/* Idempotent slot for a managed-io driver (owns its own transport, e.g. esp_lcd): allocates the
 * registry entry + driver state but does NOT add an IDF device. */
spi_dev_t *spi_dev_reserve(spi_bus_t *bus, int cs, const spi_driver_desc_t *driver, esp_err_t *err);

/* Full-duplex / half-duplex raw transfers on a raw device (add_device path). */
esp_err_t spi_dev_transfer(spi_dev_t *d, const uint8_t *tx, uint8_t *rx, size_t n);
