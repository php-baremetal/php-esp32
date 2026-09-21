/*
 * spi_driver_desc_t -- the contract a SPI device driver fills in. A raw device leaves it NULL; a
 * driver (e.g. a display panel) fills the fields it needs. Mirrors i2c_driver_desc_t on the SPI bus.
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* The per-device registry entry (spi_registry.h); a driver receives it as its handle. */
typedef struct spi_dev spi_dev_t;

/* PHP function table; only the pointer is used here, so the engine headers are not pulled in. */
typedef struct _zend_function_entry zend_function_entry;

typedef struct spi_driver_desc {
    const char *name;              /* manifest key, e.g. "st77916" */
    const char *php_class;         /* registered class, e.g. "St77916"; extends SpiDevice */
    const char *capability;        /* interface FQN this driver implements, or NULL */

    int      default_cs;           /* chip-select pin default (-1 = none) */
    uint32_t default_hz;
    uint8_t  default_mode;         /* SPI mode 0..3 */
    bool     managed_io;           /* true: the driver owns its transport (esp_lcd), not raw add_device */

    size_t   state_size;           /* per-instance state, allocated in the device registry */
    esp_err_t (*init)(spi_dev_t *d);
    void      (*deinit)(spi_dev_t *d);

    const zend_function_entry *methods;
} spi_driver_desc_t;

/* The build generates this table from the selected driver list (see CMakeLists and the manifest).
 * Empty when no drivers are selected. */
extern const spi_driver_desc_t *const spi_drivers[];
extern const size_t spi_driver_count;
