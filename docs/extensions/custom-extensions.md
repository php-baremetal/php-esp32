---
eyebrow: 'Docs · Extensions'
lede:    'Ship your own native PHP functions with a project: drop C under ./firmware/exts/<name>/, and phpflash compiles it into the firmware. A display driver, a sensor, a protocol — statically linked, registered at startup, no fork of the base firmware.'
see_also:
  - { href: './porting-status.md', meta: '6 min' }
  - { href: '../getting-started/architecture.md', meta: '8 min' }
  - { href: '../getting-started/quick-start.md', meta: '10 min' }
prev: { label: 'OPcache', href: './opcache.md' }
next: { label: 'Persistent store', href: '../storage/persistent-store.md' }
---

# Per-project C extensions

A project can ship its own PHP extensions, written in C, and have them compiled into the firmware. This is how you expose new native functions — a display driver, a sensor, a protocol — to your PHP without forking the base firmware. Drop native code under `./firmware/exts/<name>/`, run `phpflash build`, and the new functions are callable from your script.

The bundled [`oled-ssd1306-ext`](https://github.com/php-baremetal/php-esp32/blob/master/examples/oled-ssd1306-ext/README.md) example is a worked case: an SSD1306 OLED driver in C, running the same panel at roughly twice the frame rate of the same driver written in pure PHP.

Extensions are **statically linked**. There is no `dlopen` on this target, so there are no `.so` files. Everything is compiled in and registered at startup.

<!-- @callout variant="info" title="Same shape as a built-in extension" -->
A project extension is structurally identical to the firmware's built-in `gpio` extension (`components/php_ext_gpio/`). It is a normal Zend module entry with a function table; the only project-specific machinery is how the build discovers it and how `main.c` registers it. If you have written a PHP extension before, there is nothing new to learn about the C — only about the wiring.
<!-- @endcallout -->

## Quick start

From the project directory, scaffold one with `phpflash ext new`, then build:

<!-- @code-block language="bash" label="terminal — scaffold and build" -->
```bash
phpflash ext new myext      # creates firmware/exts/myext/myext.c with a working skeleton
phpflash build              # compiles it into the firmware
```
<!-- @endcode-block -->

The scaffold defines the module entry and two example functions — `myext_hello()` and `myext_add($a, $b)` — to replace with your own. After the build, phpflash prints a reminder that the extension exposes those two functions and that optional use should be guarded with `function_exists('myext_hello')`. The rest of this page is the contract behind that skeleton.

<!-- @callout variant="note" title="Run it from the project directory" -->
`phpflash ext new` expects to run where your `php-esp32.config.toml` lives — it writes into `./firmware/exts/`. If it does not find a config file in the current directory it still scaffolds, but prints a note telling you to run it from the project directory. Nothing about the scaffold depends on the config; the warning is only a guard against writing the file in the wrong place.
<!-- @endcallout -->

## The `phpflash ext new` scaffold

<!-- @steps -->
- **Name the extension** — `phpflash ext new <name>`. The name becomes both the directory and the C symbol `<name>_module_entry`, so it must be a valid C identifier: it is validated against `^[a-z][a-z0-9_]*$` (lowercase letter first, then lowercase letters, digits, and underscores). A name that does not match is rejected with an explanation.
- **File is written** — the command creates `./firmware/exts/<name>/<name>.c` from the extension template, filling `<name>` throughout. It creates the directory tree if it does not exist.
- **Existing files are protected** — if `firmware/exts/<name>/<name>.c` already exists, the command refuses rather than clobber your work. Pass `--force` to overwrite deliberately.
- **Edit the skeleton** — replace `<name>_hello()` and `<name>_add()` with your own functions, add more `*.c`/`*.h` files to the directory as needed, and list any extra ESP-IDF components in `idf_requires.txt`.
- **Build** — `phpflash build` compiles the directory into the firmware and the functions become callable from PHP.
<!-- @endsteps -->

The parent command, `phpflash ext`, is a small namespace for managing a project's custom extensions; `new` is its scaffolding subcommand.

## The generated skeleton

The template writes a complete, buildable extension. For `phpflash ext new myext` it expands to:

<!-- @code-block language="c" label="firmware/exts/myext/myext.c — generated skeleton" -->
```c
/*
 * myext: a php-esp32 project extension -- custom PHP functions written in C.
 *
 * phpflash compiles this into the firmware from ./firmware/exts/myext/. The module entry must be
 * named `myext_module_entry` (the directory name decides the symbol); the firmware registers it
 * at startup, so the functions below are callable from your PHP. Guard optional use with
 * function_exists('myext_hello'). See docs/custom-extensions.md for the full contract.
 *
 * The common hardware components (esp_driver_gpio, esp_driver_i2c, driver, esp_timer) are already
 * on the link; if you need another ESP-IDF component, add it to myext/idf_requires.txt (one
 * component name per line).
 */
#include "php.h"

/* A starter function -- replace it with your own. */
PHP_FUNCTION(myext_hello)
{
    ZEND_PARSE_PARAMETERS_NONE();
    php_printf("hello from the myext extension\n");
}

/* Example with arguments: myext_add($a, $b) returns $a + $b. */
ZEND_BEGIN_ARG_INFO_EX(arginfo_myext_add, 0, 0, 2)
    ZEND_ARG_INFO(0, a)
    ZEND_ARG_INFO(0, b)
ZEND_END_ARG_INFO()

PHP_FUNCTION(myext_add)
{
    zend_long a, b;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(a)
        Z_PARAM_LONG(b)
    ZEND_PARSE_PARAMETERS_END();
    RETURN_LONG(a + b);
}

static const zend_function_entry myext_functions[] = {
    PHP_FE(myext_hello, NULL)
    PHP_FE(myext_add,   arginfo_myext_add)
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
<!-- @endcode-block -->

Calling it from PHP, once built and flashed:

<!-- @code-block language="php" label="project-src/index.php — calling myext" -->
```php
<?php
if (function_exists('myext_hello')) {
    myext_hello();                 // prints: hello from the myext extension
    echo myext_add(20, 22), "\n";  // prints: 42
} else {
    echo "myext is not built into this firmware\n";
}
```
<!-- @endcode-block -->

The pieces, top to bottom:

- **`#include "php.h"`** pulls in the engine headers. That single include is enough for the function macros, argument parsing, and the module-entry struct.
- **`PHP_FUNCTION(name)`** defines a native function. Arguments are pulled off the stack with the `ZEND_PARSE_PARAMETERS_*` macros; return values are set with `RETURN_LONG`, `RETURN_TRUE`, `RETURN_STRING`, and friends.
- **The arginfo block** (`ZEND_BEGIN_ARG_INFO_EX` … `ZEND_END_ARG_INFO`) declares the function's parameters to the engine. The `2` at the end of `ZEND_BEGIN_ARG_INFO_EX(arginfo_myext_add, 0, 0, 2)` is the count of required arguments.
- **The function table** (`zend_function_entry[]`) lists every function the module exposes, each with `PHP_FE(function, arginfo)`, terminated by `PHP_FE_END`.
- **The module entry** ties it together. Its second field, the string `"myext"`, is the extension's reported name; the five `NULL`s are the lifecycle hooks (`MINIT`, `MSHUTDOWN`, `RINIT`, `RSHUTDOWN`, `MINFO`); `"0.1"` is the version.

## Layout

Put each extension in its own directory under `firmware/exts/`, next to your project config:

<!-- @code-block language="text" label="tree — project with one extension" -->
```text
my-project/
├── php-esp32.config.toml
├── project-src/            your PHP
└── firmware/exts/
    └── myext/
        ├── myext.c
        └── (more .c / .h)
```
<!-- @endcode-block -->

Every subdirectory of `firmware/exts/` is one extension. All of its `*.c` files are compiled, and its own directory is added to the include path — so `#include "myext.h"` for a header sitting next to `myext.c` just works. A directory with no `*.c` files is skipped.

## The contract

An extension directory `firmware/exts/<name>/` must define a Zend module entry named `<name>_module_entry`. The directory name decides the symbol — that is the whole convention. It is the same shape as the built-in `gpio` extension.

The smallest possible extension is a single no-argument function:

<!-- @code-block language="c" label="firmware/exts/myext/myext.c — minimal" -->
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
<!-- @endcode-block -->

Build and flash, and `myext_hello()` is callable from your PHP. Guard optional use with `function_exists('myext_hello')` so a script that runs on a firmware built without the extension degrades gracefully instead of fatally.

Register functions, classes, and constants in the usual places — the function table above, or an `MINIT` handler. An `MINIT` runs when the extension is registered. The built-in `gpio` extension uses one to publish its `GPIO_INPUT`/`GPIO_OUTPUT` constants:

<!-- @code-block language="c" label="components/php_ext_gpio/gpio.c — MINIT registering constants" -->
```c
PHP_MINIT_FUNCTION(gpio)
{
    REGISTER_LONG_CONSTANT("GPIO_INPUT",  0, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("GPIO_OUTPUT", 1, CONST_CS | CONST_PERSISTENT);
    return SUCCESS;
}

zend_module_entry gpio_module_entry = {
    STANDARD_MODULE_HEADER,
    "gpio",
    gpio_functions,
    PHP_MINIT(gpio),        /* the MINIT slot, instead of NULL */
    NULL, NULL, NULL, NULL, /* MSHUTDOWN, RINIT, RSHUTDOWN, MINFO */
    "0.1",
    STANDARD_MODULE_PROPERTIES
};
```
<!-- @endcode-block -->

Extensions are registered right after the engine starts and before your script runs, so their symbols — functions, classes, constants — are available for the whole run.

## Argument handling and arginfo

Reading arguments in a `PHP_FUNCTION` is done between `ZEND_PARSE_PARAMETERS_START(min, max)` and `ZEND_PARSE_PARAMETERS_END()`, with one `Z_PARAM_*` line per argument. `Z_PARAM_OPTIONAL` marks the boundary after which arguments may be omitted. `ssd1306_begin` from the OLED example takes two required pins and an optional address:

<!-- @code-block language="c" label="ssd1306.c — required + optional arguments" -->
```c
ZEND_BEGIN_ARG_INFO_EX(arginfo_begin, 0, 0, 2)
    ZEND_ARG_INFO(0, sda)
    ZEND_ARG_INFO(0, scl)
    ZEND_ARG_INFO(0, addr)
ZEND_END_ARG_INFO()

PHP_FUNCTION(ssd1306_begin)
{
    zend_long sda, scl, addr = 0x3C;   /* default for the optional arg */
    ZEND_PARSE_PARAMETERS_START(2, 3)  /* 2 required, up to 3 total */
        Z_PARAM_LONG(sda)
        Z_PARAM_LONG(scl)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(addr)
    ZEND_PARSE_PARAMETERS_END();
    /* ... */
}
```
<!-- @endcode-block -->

Strings come back as a `char *` plus a `size_t` length via `Z_PARAM_STRING(s, slen)`; longs via `Z_PARAM_LONG`. A no-argument function uses `ZEND_PARSE_PARAMETERS_NONE()` and needs no `Z_PARAM_*` lines.

<!-- @callout variant="warning" title="A no-argument function needs empty arginfo, not a shared one" -->
When a function takes no arguments, pass `NULL` as its arginfo in the function table — `PHP_FE(myext_hello, NULL)` — as the skeleton does. Do not point it at another function's arginfo block. The arginfo declares an argument count and names to the engine; reusing a two-argument block for a zero-argument function misdescribes it. The pattern to copy is the `gpio` and `ssd1306` tables: functions with arguments get their own `arginfo_*` block, functions without arguments get `NULL`.
<!-- @endcallout -->

## Extra ESP-IDF components

The extension build already links the common hardware components — `esp_driver_gpio`, `esp_driver_i2c`, `driver`, and `esp_timer` — so a GPIO or I2C driver has its headers with no extra configuration. The OLED driver, which uses the hardware I2C peripheral through `driver/i2c_master.h`, needs nothing beyond this common set.

If an extension needs something else — SPI, a filesystem, networking — list those ESP-IDF components, one per line, in `firmware/exts/<name>/idf_requires.txt`:

<!-- @code-block language="text" label="firmware/exts/myext/idf_requires.txt" -->
```text
esp_driver_spi
esp_lcd
```
<!-- @endcode-block -->

The build reads that file, deduplicates the names across all extensions, and adds them to the component's `REQUIRES`.

## How it is wired

Three pieces cooperate to turn a directory of C into registered PHP functions.

### phpflash passes the directory to the build

phpflash detects `./firmware/exts/` and passes `-DPHP_PROJECT_EXTS_DIR=<abs path>` to the ESP-IDF build. With no such directory the flag is absent and nothing about the firmware changes.

### php_project_exts globs, compiles, and generates a table

The `php_project_exts` component reads `PHP_PROJECT_EXTS_DIR`, globs every subdirectory, collects each one's `*.c` sources, adds each directory to the include path, and reads any `idf_requires.txt`. It then generates a small C registration table, `project_exts_register.c`, listing the module entries:

<!-- @code-block language="c" label="build output — project_exts_register.c (generated)" -->
```c
/* Generated by components/php_project_exts/CMakeLists.txt -- do not edit. */
#include "zend_modules.h"

extern zend_module_entry ssd1306_module_entry;

zend_module_entry * const php_esp32_project_extensions[] = {
    &ssd1306_module_entry,
};
const int php_esp32_project_extension_count = 1;
```
<!-- @endcode-block -->

The component always defines the count — `0` when there are no extensions — but only defines the array when there is at least one entry. The component registration wires up the sources, the include dirs, and the requires, and forces the archive in whole:

<!-- @code-block language="cmake" label="components/php_project_exts/CMakeLists.txt — registration" -->
```cmake
idf_component_register(
    SRCS ${_ext_srcs} "${_gen}"
    INCLUDE_DIRS ${_ext_incs}
    # php for the engine headers; the common hardware components so a driver
    # extension has its headers; plus anything from an idf_requires.txt.
    REQUIRES php esp_driver_gpio esp_driver_i2c driver esp_timer ${_ext_extra_reqs}
    # The module entries are reached only through the generated table, itself
    # referenced weakly from main.c; force the whole archive in so --gc-sections
    # can't drop them (same as php_ext_gpio).
    WHOLE_ARCHIVE
)

# Extension sources lean on PHP's macro-heavy headers; keep them under the same
# relaxed flags as php_ext_gpio and the engine sources.
target_compile_options(${COMPONENT_LIB} PRIVATE -w -fpermissive)
```
<!-- @endcode-block -->

The `WHOLE_ARCHIVE` is load-bearing. The module entries are reached only through the generated table, which is itself referenced weakly from `main.c`; without forcing the whole archive in, the linker's `--gc-sections` would see no direct reference and drop the entries. The built-in `gpio` extension is linked the same way for the same reason.

### main.c registers each entry after the engine starts

`main.c` declares the generated table and count as **weak** symbols, so a firmware built with no project extensions still links — the count then resolves to `0` and the loop does nothing:

<!-- @code-block language="c" label="main/main.c — weak table + registration" -->
```c
extern zend_module_entry * const php_esp32_project_extensions[] __attribute__((weak));
extern const int php_esp32_project_extension_count __attribute__((weak));

static void register_project_extensions(void)
{
    if (&php_esp32_project_extension_count == NULL || php_esp32_project_extension_count == 0) {
        return;
    }
    for (int i = 0; i < php_esp32_project_extension_count; i++) {
        zend_module_entry *m = php_esp32_project_extensions[i];
        if (zend_startup_module(m) == SUCCESS) {
            ESP_LOGI(TAG, "project ext '%s' registered", m->name);
        } else {
            ESP_LOGW(TAG, "project ext '%s' failed to register", m->name);
        }
    }
}
```
<!-- @endcode-block -->

The call happens right after `php_embed_init()`, before the script runs:

<!-- @code-block language="c" label="main/main.c — call site" -->
```c
if (php_embed_init(0, NULL) != SUCCESS) {
    ESP_LOGE(TAG, "php_embed_init failed");
    vTaskDelete(NULL);
    return;
}

/* Register any per-project C extensions (from ./firmware/exts) before the script runs. */
register_project_extensions();
```
<!-- @endcode-block -->

Each successful registration logs `project ext '<name>' registered` to the serial console, which is a quick way to confirm on boot that your extension made it into the image.

## Worked example: the SSD1306 native driver

The [`oled-ssd1306-ext`](https://github.com/php-baremetal/php-esp32/blob/master/examples/oled-ssd1306-ext/README.md) example drives a 0.91" SSD1306 128x32 OLED entirely from C. The I2C link (the ESP32 hardware I2C peripheral), the framebuffer, and the text rendering all live in the extension; PHP just calls the `ssd1306_*` functions. It is the native counterpart to `oled-ssd1306-fps`, where the same panel is driven by a pure-PHP bit-banged I2C driver, and the point of the pair is to compare frame rate.

### Project layout

<!-- @code-block language="text" label="tree — oled-ssd1306-ext" -->
```text
oled-ssd1306-ext/
├── php-esp32.config.toml
├── project-src/
│   └── index.php               the demo, calling ssd1306_*
└── firmware/exts/
    └── ssd1306/
        └── ssd1306.c           defines zend_module_entry ssd1306_module_entry
```
<!-- @endcode-block -->

The project is `storage_type = "embedded"` and `microsd = false` — a pure-flash image with the script baked in, so it runs straight from reset with no card and no network.

### The extension's function surface

The C driver exposes a compact drawing API, registered in the module's function table:

<!-- @code-block language="c" label="ssd1306.c — function table" -->
```c
static const zend_function_entry ssd1306_functions[] = {
    PHP_FE(ssd1306_begin,   arginfo_begin)   /* ssd1306_begin(sda, scl, addr = 0x3C): bool */
    PHP_FE(ssd1306_present, NULL)            /* ssd1306_present(): bool -- does the panel ACK? */
    PHP_FE(ssd1306_clear,   NULL)            /* clear the framebuffer */
    PHP_FE(ssd1306_pixel,   arginfo_pixel)   /* set one pixel */
    PHP_FE(ssd1306_rect,    arginfo_rect)    /* fill a rectangle */
    PHP_FE(ssd1306_text,    arginfo_text)    /* draw 5x7 text */
    PHP_FE(ssd1306_flush,   NULL)            /* push the framebuffer to the panel: bool */
    PHP_FE_END
};

zend_module_entry ssd1306_module_entry = {
    STANDARD_MODULE_HEADER,
    "ssd1306",
    ssd1306_functions,
    NULL, NULL, NULL, NULL, NULL,
    "0.1",
    STANDARD_MODULE_PROPERTIES,
};
```
<!-- @endcode-block -->

Note the mix: functions that take arguments (`ssd1306_begin`, `ssd1306_pixel`, `ssd1306_rect`, `ssd1306_text`) carry their own `arginfo_*` block, while the no-argument functions (`ssd1306_present`, `ssd1306_clear`, `ssd1306_flush`) pass `NULL` — the pattern from the arginfo gotcha above, applied across a real surface.

The framebuffer and I2C handles are plain C statics inside the extension. `ssd1306_flush` sets the SSD1306 addressing window and pushes all 512 bytes of the 128x32 buffer in one `i2c_master_transmit` at 400 kHz I2C fast mode.

### Calling it from PHP

The demo is an `init-loop` sketch. `setup()` brings up the panel and prints a banner; `loop()` renders and flushes as many full frames as it can inside a fixed time window, then reports the throughput:

<!-- @code-block language="php" label="project-src/index.php — the FPS loop (excerpt)" -->
```php
<?php
const SDA = 7;      // board pin "SDA / GPIO7"
const SCL = 8;      // board pin "SCL / GPIO8"

function setup(): void
{
    if (!function_exists('ssd1306_begin')) {
        echo "ERROR: the ssd1306 extension is not built in -- check firmware/exts/ssd1306/\n";
        return;
    }
    ssd1306_begin(SDA, SCL, 0x3C);
    echo ssd1306_present() ? "panel: present (ACK)\n" : "panel: no ACK -- check wiring\n";
    ssd1306_clear();
    ssd1306_text(4, 4,  'PHP ' . PHP_MAJOR_VERSION . '.' . PHP_MINOR_VERSION);
    ssd1306_text(4, 18, 'C EXT');
    ssd1306_flush();
}

function loop(int $tick): void
{
    global $fps;
    $count = 0;
    $t0 = microtime(true);
    do {                              // everything in here runs in C
        ssd1306_clear();
        ssd1306_text(0, 0, 'SSD1306 C EXT');
        ssd1306_text(0, 18, 'FPS ' . sprintf('%5.1f', $fps));
        ssd1306_flush();
        $count++;
        $elapsed = microtime(true) - $t0;
    } while ($elapsed < 0.30);
    $fps = $count / $elapsed;
    delay(10);   // yield one tick so the task watchdog stays fed
}
```
<!-- @endcode-block -->

The `delay()` call here is not from the ssd1306 extension — it is one of the built-in `gpio` extension's functions, and it yields the core through `vTaskDelay` so the FreeRTOS watchdog stays satisfied during a hot loop.

### The result

On the ESP32-P4-ETH (PHP 8.4, 360 MHz), full 128x32 frames of 512 bytes each:

| Driver | FPS | Bound by |
|---|---|---|
| Pure PHP, bit-banged I2C (`oled-ssd1306-fps`) | ~41 | the PHP interpreter (per-edge call overhead) |
| Native C, hardware I2C (this example) | ~82 | the I2C bus (400 kHz fast mode) |

The C driver is exactly 2x faster and now runs into the I2C bus limit rather than the CPU: at 400 kHz, 512 bytes plus the addressing take about 12 ms, so ~82 full frames a second is close to the theoretical ceiling for the panel. The pure-PHP version never gets near it — the interpreter caps the effective clock long before the bus does.

## Limits and gotchas

<!-- @callout variant="warning" title="Static only — no runtime loading" -->
There is no `dlopen` and no runtime loading on this target. To change an extension you rebuild and reflash. There is no faster loop; the C is part of the firmware image.
<!-- @endcallout -->

<!-- @callout variant="note" title="Registration timing: MINIT runs, per-request RINIT does not" -->
Extensions are added just after the engine starts, so `MINIT` runs and the module's functions, classes, and constants are available for the run. A per-request `RINIT`/`RSHUTDOWN` is not called for a module added this late — a hardware-driver extension does not need one. In the `web-server` execution model each HTTP request is a fresh PHP request, but the module is registered once at boot. Keep per-request state out of the extension, or reinitialise it from PHP at the start of each request.
<!-- @endcallout -->

<!-- @callout variant="warning" title="Write extensions in C, under relaxed flags" -->
The engine headers are C. Extensions compile under the same relaxed flags as the engine — `-w -fpermissive` — because PHP's macro-heavy headers do not survive strict warnings-as-errors. Write extensions in C, matching the bundled `gpio` and `ssd1306` examples. Do not rely on C++ features or on warnings catching mistakes for you; the flags suppress them.
<!-- @endcallout -->

<!-- @callout variant="tip" title="Naming rules are strict for a reason" -->
The extension name is a lowercase C identifier (`^[a-z][a-z0-9_]*$`) because it is used verbatim in two places: the directory on disk and the generated symbol `<name>_module_entry`. Keep the module-entry name and the directory name identical — the build derives one from the other, and a mismatch means the generated table references a symbol that does not exist, and the link fails.
<!-- @endcallout -->

## Next steps

- See which extensions ship built-in, optional, or unported in the [porting status](./porting-status.md).
- Understand where extensions sit in the boot and memory picture in [architecture](../getting-started/architecture.md).
- Persist values a native driver produces across reboots with the [persistent store](../storage/persistent-store.md).
