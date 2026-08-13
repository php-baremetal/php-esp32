# Per-project C extensions

A project can ship its own PHP extensions, written in C, and have them compiled into the firmware.
This is how you expose new native functions (a display driver, a sensor, a protocol) to your PHP
without forking the firmware. The [`oled-ssd1306-ext`](../examples/oled-ssd1306-ext/) example is a
worked case: an SSD1306 OLED driver in C, roughly twice the frame rate of the same driver written in
pure PHP.

Extensions are **statically linked** -- there is no `dlopen` on this target, so there are no `.so`
files. Everything is compiled in and registered at startup.

## Quick start

From the project directory, scaffold one:

```sh
phpflash ext new myext      # creates firmware/exts/myext/myext.c with a working skeleton
phpflash build              # compiles it into the firmware
```

The skeleton defines the module entry and two example functions (`myext_hello()` and
`myext_add($a, $b)`) to replace with your own. The rest of this document is the contract behind it.

## Layout

Put each extension in its own directory under `firmware/exts/` next to your project config:

```
my-project/
├── php-esp32.config.toml
├── project-src/            # your PHP
└── firmware/exts/
    └── myext/
        ├── myext.c
        └── (more .c / .h)
```

Every subdirectory of `firmware/exts/` is one extension. All its `*.c` files are compiled; its own
directory is on the include path (so `#include "myext.h"` works).

## The contract

An extension directory `firmware/exts/<name>/` must define a Zend module entry named
`<name>_module_entry` -- the directory name decides the symbol. That is the whole convention; it is
the same shape as the built-in `gpio` extension (`components/php_ext_gpio/`).

A minimal extension:

```c
#include "php.h"

PHP_FUNCTION(myext_hello)
{
    ZEND_PARSE_PARAMETERS_NONE();
    php_printf("hello from C\n");
}

static const zend_function_entry myext_functions[] = {
    PHP_FE(myext_hello, NULL)
    PHP_FE_END
};

zend_module_entry myext_module_entry = {
    STANDARD_MODULE_HEADER,
    "myext",
    myext_functions,
    NULL, NULL, NULL, NULL, NULL,   /* MINIT, MSHUTDOWN, RINIT, RSHUTDOWN, MINFO */
    "0.1",
    STANDARD_MODULE_PROPERTIES,
};
```

Build and flash, and `myext_hello()` is callable from your PHP. Guard optional use with
`function_exists('myext_hello')`.

Register functions, classes and constants in the usual places -- the function table above, or a
`MINIT` handler. A `MINIT` runs when the extension is registered. Extensions are registered right
after the engine starts and before your script runs, so their symbols are available for the whole
run.

## Extra ESP-IDF components

The extension build already links the common hardware components (`esp_driver_gpio`,
`esp_driver_i2c`, `driver`, `esp_timer`), so a GPIO or I2C driver has its headers. If an extension
needs something else -- SPI, a filesystem, networking -- list those components, one per line, in
`firmware/exts/<name>/idf_requires.txt`:

```
esp_driver_spi
esp_lcd
```

## How it is wired

- **phpflash** detects `./firmware/exts/` and passes `-DPHP_PROJECT_EXTS_DIR=<abs path>` to the
  build. With no such directory the flag is absent and nothing changes.
- The **`php_project_exts`** component globs the extension directories, compiles their sources
  (`REQUIRES php` plus the common hardware components and any `idf_requires.txt` extras), and
  generates a small registration table. It is linked `WHOLE_ARCHIVE` so the module entries -- reached
  only through the generated table -- survive `--gc-sections`.
- **`main.c`** registers each entry with `zend_startup_module()` after `php_embed_init()`. The
  symbols are weak, so a firmware built with no project extensions still links (the count is 0).

## Limits

- **Static only.** No `dlopen`, no runtime loading. Rebuild and reflash to change an extension.
- **Registration timing.** Extensions are added just after the engine starts, so `MINIT` runs and
  functions/classes/constants are available for the run. A per-request `RINIT`/`RSHUTDOWN` is not
  called for a module added this late; a hardware-driver extension does not need one. (In the
  `web-server` model each HTTP request is a fresh PHP request, but the module is registered once at
  boot -- keep per-request state out of the extension, or reinitialise it from PHP.)
- **The engine headers are C.** Extensions compile under the same relaxed flags as the engine
  (`-w -fpermissive`); write them in C, matching the bundled `gpio` and `ssd1306` examples.
