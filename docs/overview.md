---
eyebrow: 'Docs · Overview'
lede:    'The firmware that is the PHP port: the real, unmodified Zend engine from php.net, cross-compiled for the ESP32 and running your index.php on the chip — the same interpreter, opcode by opcode.'

see_also:
  - { href: './getting-started/quick-start.md', meta: '10 min' }
  - { href: './getting-started/architecture.md', meta: '8 min' }
  - { href: 'https://github.com/php-baremetal/php-esp32', meta: 'external', label: 'php-baremetal/php-esp32 on GitHub' }

prev: { label: 'No previous page', href: '#' }
next: { label: 'Getting started',  href: './getting-started/quick-start.md' }
---

# Overview

`php-esp32` runs the **official PHP interpreter on microcontrollers**. It is not a reimplementation,
a language subset, or a transpiler: the upstream Zend engine from [php.net](https://www.php.net) is
cross-compiled for the chip and executes your `index.php` opcode by opcode, the same way it would
behind a web server.

The source tree is vendored unchanged; the only modifications are separate patches applied at build
time, for the handful of places where a bare-metal RTOS differs from a Unix host. Hand the same
script to this engine or to a desktop `php` and you get the same output — because underneath it is
the same interpreter.

<!-- @callout variant="info" title="What real PHP means here" -->
Namespaces, classes, interfaces, traits, enums, closures, generators, `match`, exceptions, typed
properties and attributes — plus strings, arrays, math, JSON, PCRE, hashing, SPL, Reflection and the
CSPRNG. Composer works with unmodified packages from Packagist, and on the ESP32-P4 stock **Laravel**
and **Symfony** serve pages over HTTP.
<!-- @endcallout -->

## The shape of a project

A project is a folder with a small `php-esp32.config.toml` and a `project-src/` holding your PHP. You
write the code; [`phpflash`](https://github.com/php-baremetal/flash-tool) scaffolds, builds, flashes
and monitors it.

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
// runs once at boot
function setup(): void {
    gpio_mode(2, GPIO_OUTPUT);
}

// runs forever, Arduino-style
function loop(): void {
    gpio_write(2, 1); delay(500);
    gpio_write(2, 0); delay(500);
}
```
<!-- @endcode-block -->

There are three execution models — a plain run-once script, the `setup()` / `loop()` sketch above,
and a per-request `web-server` model — covered in [Getting started](./getting-started/quick-start.md).

## Hardware today

| Family | Core | PSRAM | Networking |
| ------ | ---- | ----- | ---------- |
| **ESP32-P4** | dual-core RISC-V, up to 400 MHz | up to 32 MB | Ethernet on the `-eth` boards |
| **ESP32-S3** | dual-core Xtensa LX7, 240 MHz | 8 MB | Ethernet (W5500) on the `-eth` boards |

PHP **8.3, 8.4 and 8.5** coexist in the tree and are selectable per project. A new chip family is a
directory under `boards/`, not a change to the engine.

## Where to go next

- **[Getting started](./getting-started/quick-start.md)** — install the tools, scaffold, flash your first board.
- **[Architecture](./getting-started/architecture.md)** — memory, the execution models, how PHP is embedded.
- **[Extensions](./extensions/porting-status.md)** — what is built in, what is optional, what is not ported.
- **[Storage & state](./storage/persistent-store.md)** — microSD vs embedded, the `store_*` and `mem_*` KV stores, and a baked-in `.env`.
- **[Reference](./reference/footprint.md)** — flash/RAM footprint and the deep porting notes.
