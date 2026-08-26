---
eyebrow: 'Docs · Recipes'
lede:    'Drive a 0.91" I2C SSD1306 OLED from PHP on the ESP32-P4-ETH. Two grounded drivers for the same panel: a pure-PHP bit-banged one at ~41 FPS, and a native C extension at ~82 FPS.'
see_also:
  - { href: '../extensions/custom-extensions.md', meta: '12 min' }
  - { href: './microsd-files.md', meta: '6 min' }
  - { href: '../getting-started/quick-start.md', meta: '10 min' }
prev: { label: 'Read a button', href: './read-a-button.md' }
next: { label: 'Read and write the microSD', href: './microsd-files.md' }
---

# Drive an SSD1306 OLED from PHP

A 0.91" SSD1306 (128x32, I2C) is the smallest useful display you can hang off the board, and there are two ways to drive it from PHP on this target. One writes the whole I2C link — every clock edge — in PHP over two GPIO pins. The other puts the driver in a native C extension and lets the hardware I2C peripheral do the clocking. Both draw the same framebuffer and the same 5x7 text; the difference is frame rate, and the gap is exactly 2x.

Both are shipped as worked examples: [`oled-ssd1306-fps`](../../examples/oled-ssd1306-fps/README.md) (pure PHP) and [`oled-ssd1306-ext`](../../examples/oled-ssd1306-ext/README.md) (C extension).

## Goal

Put text and graphics on a 0.91" SSD1306 128x32 OLED, refreshing full frames as fast as the driver allows, and print the measured frame rate to both the serial console and the panel itself.

## What you need

- A 0.91" SSD1306 128x32 OLED module, I2C, answering at `0x3C` (some answer at `0x3D`).
- An ESP32-P4-ETH board. It breaks out a labelled I2C header — `SDA / GPIO7` and `SCL / GPIO8` — and both examples use it.
- Four wires: `VCC → 3V3`, `GND → GND`, `SDA → GPIO7`, `SCL → GPIO8`.

<!-- @code-block language="text" label="wiring — SSD1306 to ESP32-P4-ETH" -->
```text
OLED    Board (ESP32-P4-ETH)
VCC  →  3V3
GND  →  GND
SDA  →  GPIO7   (board pin "SDA / GPIO7")
SCL  →  GPIO8   (board pin "SCL / GPIO8")
```
<!-- @endcode-block -->

The 0.91" modules carry their own SDA/SCL pull-ups, so the bus idles high without any external resistors.

<!-- @callout variant="warning" title="Use GPIO7 and GPIO8 — GPIO9 is not on the header" -->
On the ESP32-P4-ETH the labelled I2C header exposes `SDA / GPIO7` and `SCL / GPIO8`. GPIO9 is not broken out on that header, so do not reach for it as a third I2C line. Stick to SDA=7 and SCL=8; change the pins at the top of `project-src/index.php` only if you are wiring the panel somewhere else.
<!-- @endcallout -->

## Two ways

The pure-PHP driver bit-bangs I2C with the built-in `gpio` extension — no C-side I2C driver, so every START, clock edge and byte is a PHP call. That is what makes it a language benchmark. The C extension moves the whole driver — the I2C link, the framebuffer, and the text rendering — into native code under `firmware/exts/ssd1306/`, and PHP just calls the `ssd1306_*` functions.

<!-- @tabs labels="Pure PHP, C extension" -->
<!-- @tab index="0" -->
The reusable `SSD1306` class in `project-src/SSD1306.php` holds the framebuffer and drives the bus. SCL is a permanent push-pull output; the eight data bits drive SDA push-pull, and the ninth (ACK) clock drives SDA low as well — a write-only master that ignores the ACK. Keeping SDA an output for the whole transfer avoids a slow pin-direction change on every byte, which matters when the loop is interpreted PHP. `present()` is the one exception: it releases SDA and reads a real ACK, to check the panel is actually on the bus.

<!-- @code-block language="php" label="project-src/SSD1306.php — the bit-bang core (excerpt)" -->
```php
private function busInit(): void
{
    gpio_mode($this->scl, GPIO_OUTPUT); gpio_write($this->scl, 1);
    gpio_mode($this->sda, GPIO_OUTPUT); gpio_write($this->sda, 1);
}

/** Clock out one byte, MSB first; the ninth clock is a driven-low fake ACK. */
private function writeByte(int $v): void
{
    $sda = $this->sda;
    $scl = $this->scl;
    for ($i = 0; $i < 8; $i++) {
        gpio_write($sda, ($v >> 7) & 1);
        $v = ($v << 1) & 0xFF;
        gpio_write($scl, 1);
        gpio_write($scl, 0);
    }
    gpio_write($sda, 0);       // ninth clock, SDA held low
    gpio_write($scl, 1);
    gpio_write($scl, 0);
}
```
<!-- @endcode-block -->

A frame is the full 512-byte framebuffer (128 x 32 / 8) pushed in a single I2C data transaction. Measured on the ESP32-P4-ETH (PHP 8.4, 360 MHz), this holds around **41 FPS** — bound by the per-edge PHP call overhead, not the panel.
<!-- @endtab -->
<!-- @tab index="1" -->
The driver lives in `firmware/exts/ssd1306/ssd1306.c` and uses the ESP32 hardware I2C peripheral through `driver/i2c_master.h`. phpflash compiles everything under a project's `firmware/exts/` into the firmware and registers it after `php_embed_init()`, so the `ssd1306_*` functions are callable from your script. Nothing about the base firmware changes. `ssd1306_flush` sets the addressing window and pushes all 512 bytes in one `i2c_master_transmit` at 400 kHz I2C fast mode.

<!-- @code-block language="c" label="firmware/exts/ssd1306/ssd1306.c — bring up I2C + init (excerpt)" -->
```c
PHP_FUNCTION(ssd1306_begin)
{
    zend_long sda, scl, addr = 0x3C;
    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_LONG(sda)
        Z_PARAM_LONG(scl)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(addr)
    ZEND_PARSE_PARAMETERS_END();

    s_addr = (uint16_t) addr;

    if (s_bus == NULL) {
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = -1,                    /* auto-pick a free controller */
            .sda_io_num = (int) sda,
            .scl_io_num = (int) scl,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags = { .enable_internal_pullup = true },
        };
        if (i2c_new_master_bus(&bus_cfg, &s_bus) != ESP_OK) {
            RETURN_FALSE;
        }
    }
    /* ... add device at 400 kHz, then ssd_init_panel() ... */
    RETURN_TRUE;
}
```
<!-- @endcode-block -->

The module's function table exposes the whole drawing API — `ssd1306_begin`, `ssd1306_present`, `ssd1306_clear`, `ssd1306_pixel`, `ssd1306_rect`, `ssd1306_text`, `ssd1306_flush` — as `ssd1306_module_entry`. On the same hardware this hits **~82 FPS**, now bound by the 400 kHz I2C bus rather than the interpreter. See [Per-project C extensions](../extensions/custom-extensions.md) for the full contract behind `firmware/exts/`.
<!-- @endtab -->
<!-- @endtabs -->

<!-- @callout variant="tip" title="Which one to reach for" -->
Start in pure PHP: there is nothing to compile beyond the normal image, the driver is one `require` away, and ~41 FPS is more than enough for a status readout or a slow gauge. Move to the C extension when you need the throughput — an animation, a fast-updating meter — or when you want the drawing loop off the interpreter's critical path. The C driver runs into the bus limit, so it is as fast as this panel can go.
<!-- @endcallout -->

## The code

Both examples are `init-loop` sketches: `setup()` brings the panel up and shows a splash, `loop()` renders and flushes as many full frames as it can inside a fixed time window and reports the throughput.

<!-- @tabs labels="Pure PHP, C extension" -->
<!-- @tab index="0" -->
<!-- @code-block language="php" label="project-src/index.php — pure-PHP driver" -->
```php
<?php
require __DIR__ . '/SSD1306.php';

const SDA = 7;      // board pin "SDA / GPIO7"
const SCL = 8;      // board pin "SCL / GPIO8"

$oled = null;
$fps  = 0.0;

function setup(): void
{
    global $oled;
    $oled = new SSD1306(SDA, SCL, 0x3C);
    echo $oled->present()
        ? "panel: present (ACK)\n"
        : "panel: no ACK -- check wiring/address; driving blind\n";

    $oled->begin();
    $oled->clear();
    $oled->text(4, 4,  'PHP ' . PHP_MAJOR_VERSION . '.' . PHP_MINOR_VERSION);
    $oled->text(4, 18, 'OLED FPS');
    $oled->flush();
    delay(800);
}

function loop(int $tick): void
{
    global $oled, $fps;
    static $sweep = 0;

    $count = 0;
    $t0 = microtime(true);
    do {                                  // all real drawing, no delay
        $oled->clear();
        $oled->text(0, 18, 'FPS ' . sprintf('%4.1f', $fps));
        $oled->rect($sweep++ % (SSD1306::WIDTH - 14), 27, 14, 5);
        $oled->flush();
        $count++;
        $elapsed = microtime(true) - $t0;
    } while ($elapsed < 0.30);

    $fps = $count / $elapsed;
    printf("draw fps: %5.1f  (%d frames in %d ms)\n",
        $fps, $count, (int) round($elapsed * 1000));
    delay(10);   // yield one tick so the task watchdog stays fed
}
```
<!-- @endcode-block -->
<!-- @endtab -->
<!-- @tab index="1" -->
<!-- @code-block language="php" label="project-src/index.php — native C driver" -->
```php
<?php
const SDA = 7;      // board pin "SDA / GPIO7"
const SCL = 8;      // board pin "SCL / GPIO8"

$fps = 0.0;

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
    delay(800);
}

function loop(int $tick): void
{
    global $fps;
    static $sweep = 0;

    $count = 0;
    $t0 = microtime(true);
    do {                                  // everything in here runs in C
        ssd1306_clear();
        ssd1306_text(0,  0, 'SSD1306 C EXT');
        ssd1306_text(0, 18, 'FPS ' . sprintf('%5.1f', $fps));
        ssd1306_rect($sweep++ % (128 - 14), 27, 14, 5);
        ssd1306_flush();
        $count++;
        $elapsed = microtime(true) - $t0;
    } while ($elapsed < 0.30);

    $fps = $count / $elapsed;
    printf("draw fps: %6.1f  (%d frames in %d ms)\n",
        $fps, $count, (int) round($elapsed * 1000));
    delay(10);   // yield one tick so the task watchdog stays fed
}
```
<!-- @endcode-block -->

The C-extension script guards with `function_exists('ssd1306_begin')` so it degrades gracefully on a firmware built without the extension instead of fatally.
<!-- @endtab -->
<!-- @endtabs -->

<!-- @callout variant="note" title="delay() is from the gpio extension, not ssd1306" -->
The `delay()` call in both loops is one of the built-in `gpio` extension's functions. It yields the core through `vTaskDelay` so the FreeRTOS task watchdog stays satisfied during a hot render loop — always yield at least one tick per burst.
<!-- @endcallout -->

Both projects use the same config: a pure-flash image with the script baked in, no microSD and no network, so it runs straight from reset.

<!-- @code-block language="toml" label="php-esp32.config.toml — both examples" -->
```toml
name = "oled-ssd1306-fps"
storage_type = "embedded"   # baked into flash; the demo needs no microSD
type = "init-loop"          # setup()/loop() sketch, Arduino-style

[board]
target = "esp32-p4-eth"
port   = ""          # empty = autodetect at flash time

[storage]
microsd = false      # pure flash: the SD stack isn't compiled in

[php]
src     = "project-src"
entry   = "index.php"
version = ""         # empty = repo default PHP version
```
<!-- @endcode-block -->

For the C-extension version the only difference on disk is the driver directory:

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

Each subdirectory of `firmware/exts/` is one extension; its `*.c` files are compiled in and it must define `<dirname>_module_entry`. This driver needs no extra ESP-IDF components — the common set (`esp_driver_gpio`, `esp_driver_i2c`, `driver`, `esp_timer`) already covers hardware I2C.

## Build & flash

Same three commands for either version — for the C-extension project, `phpflash build` compiles `firmware/exts/ssd1306` into the image automatically.

<!-- @code-block language="bash" label="terminal — build, flash, watch" -->
```bash
phpflash build      # ESP32-P4-ETH; PHP (and any firmware/exts/) baked into the image
phpflash flash
phpflash monitor
```
<!-- @endcode-block -->

For the C extension, the boot log confirms the driver made it into the image with a line like `project ext 'ssd1306' registered`.

## What you'll see

`setup()` probes the panel and shows a splash, then `loop()` benchmarks full-frame flushes and prints the rate each burst. The measured FPS is also drawn on the panel, next to a block that sweeps left-right one step per frame so the refresh is visible.

<!-- @code-block language="text" label="serial monitor — pure-PHP driver" -->
```text
SSD1306 128x32 over PHP-bit-banged I2C (SDA=7 SCL=8)
panel: present (ACK)
benchmarking full-frame flushes...
draw fps:  41.2  (13 frames in 316 ms)
draw fps:  41.0  (13 frames in 317 ms)
```
<!-- @endcode-block -->

<!-- @code-block language="text" label="serial monitor — native C driver" -->
```text
SSD1306 via native C extension (SDA=7 SCL=8)
panel: present (ACK)
benchmarking full-frame flushes (native driver)...
draw fps:   82.0  (25 frames in 305 ms)
draw fps:   82.3  (25 frames in 304 ms)
```
<!-- @endcode-block -->

The pure-PHP driver holds around 41 FPS; the native C driver is exactly 2x faster at ~82 FPS. At 400 kHz, 512 bytes plus the addressing take about 12 ms, so ~82 full frames a second is close to the theoretical ceiling for this panel — the C driver runs into the bus, the pure-PHP one into the interpreter.

If the panel reports `no ACK`, check the wiring and the address (`0x3D` on some modules) — the pure-PHP demo keeps measuring FPS even while driving blind, so a running FPS counter with no picture points straight at the bus.
