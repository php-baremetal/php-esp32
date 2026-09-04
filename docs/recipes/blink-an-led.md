---
eyebrow: 'Docs · Recipes'
lede:    'Drive a GPIO pin from PHP to blink an LED, using the setup()/loop() model with gpio_mode, gpio_write and delay — the smallest end-to-end program that proves PHP is moving real hardware.'
see_also:
  - { href: './read-a-button.md', meta: '5 min' }
  - { href: '../getting-started/quick-start.md', meta: '10 min' }
  - { href: '../extensions/porting-status.md', meta: '6 min' }
prev: { label: 'Build-time environment', href: '../storage/environment.md' }
next: { label: 'Read a button', href: './read-a-button.md' }
---

# Blink an LED

This is the smallest program that does something physical: PHP toggles a GPIO pin twice a second and an LED follows. It uses the `init-loop` execution model — you write `setup()` and `loop()`, Arduino-style, and the C runtime calls them. `setup()` runs once, then `loop($tick)` repeats forever, driven from C so the FreeRTOS watchdog stays fed and memory housekeeping happens between passes.

## What you need

- A board flashed with php-esp32 (the examples target the ESP32-P4-Pico).
- An LED and a series resistor of about 330 ohm.
- Two jumper wires.

Wire the LED in series with the resistor between **GPIO2** and **GND**: the pin drives the anode (through the resistor), the cathode goes to ground. Reset the board and the LED blinks.

<!-- @callout variant="warning" title="Pick a red LED and use the resistor" -->
The GPIO drives 3.3V, not 5V. A red LED with a ~330 ohm resistor is clearly visible; a blue or white LED has a higher forward voltage and can be too dim to see at 3.3V. The series resistor is not optional — it limits the current through the LED and the pin. Any free GPIO works; change the `LED` constant if GPIO2 is taken on your board.
<!-- @endcallout -->

## The code

Put this in `project-src/index.php`. It defines the pin, makes it an output in `setup()`, and toggles it every 500 ms in `loop()` by writing `$tick % 2` — high on odd ticks, low on even.

<!-- @code-block language="php" label="project-src/index.php" demo="/documentation/php-esp32/master/assets/blink.svg" demo-alt="The LED on GPIO2 blinking on and off every 500 ms, driven from PHP." -->
```php
<?php

define('LED', 2);

function setup(): void {
    gpio_mode(LED, GPIO_OUTPUT);
    echo "setup: blinking an LED on GPIO " . LED . " from PHP " . PHP_VERSION . "\n";
}

function loop(int $tick): void {
    gpio_write(LED, $tick % 2);        // on for odd ticks, off for even
    if ($tick % 10 === 0) {
        echo "tick $tick\n";
    }
    delay(500);
}
```
<!-- @endcode-block -->

Three built-in calls do all the hardware work, from the `gpio` extension:

- `gpio_mode(pin, GPIO_OUTPUT)` — set the pin direction. The constants are `GPIO_INPUT` and `GPIO_OUTPUT`.
- `gpio_write(pin, level)` — drive the pin high (non-zero) or low (0).
- `sys_delay(ms)` — sleep for `ms` milliseconds. It yields the core (it is not a busy-wait), so the watchdog stays happy through long pauses. `delay(ms)` is a plain alias, kept for the Arduino-style idiom. (Timing lives in the `sys` extension, not `gpio`.)

The full list of GPIO (and other built-in) functions is in the [built-in extension API](../extensions/builtin-api.md) reference.

## Config

A minimal `init-loop` project. The PHP source lives on the microSD; the entry file is `index.php` inside `project-src`.

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
name = "led-blink"
storage_type = "microsd"   # where the PHP source lives
type = "init-loop"         # execution model

[board]
target = "esp32-p4-pico"
port   = ""                # empty = autodetect at flash time

[esp-idf]
path    = ""
version = ""

[php-esp32]
path    = ""
version = ""

[php]
src   = "project-src"      # PHP source folder (copied to the microSD / embedded)
entry = "index.php"        # entry file within src
```
<!-- @endcode-block -->

## Build & flash

Build the firmware, write it to the board, then open the serial monitor.

<!-- @code-block language="bash" label="build, flash, monitor" -->
```bash
phpflash build && phpflash flash && phpflash monitor
```
<!-- @endcode-block -->

<!-- @callout variant="tip" title="Iterate without reflashing" -->
With `storage_type = "microsd"` the PHP source is read from the card at boot. To try a change, copy `project-src/index.php` to the card root as `index.php` and press reset — no rebuild or reflash needed.
<!-- @endcallout -->

## What you'll see

The LED blinks at 1 Hz (500 ms on, 500 ms off) and the serial log prints the banner, the `setup()` line, then a `tick` every ten passes. The free heap stays flat tick after tick — the engine runs continuously without leaking.

<!-- @code-block language="text" label="serial output (excerpt)" -->
```text
php-esp32: microSD mounted at /sdcard
php-esp32: php_embed_init()...
PHP 8.3.32 on ESP32-P4
--- /sdcard/index.php ---
setup: blinking an LED on GPIO 2 from PHP 8.3.32
php-esp32: entering loop()
tick 0
php-esp32: tick 0 -- heap free: 32694575 bytes
tick 10
```
<!-- @endcode-block -->

## Next

For real logic between blinks — a helper function and a `foreach` over a pattern array — see the `blink-sos` example, which spells "SOS" in Morse on the same pin. To read a pin instead of driving one, continue to [Read a button](./read-a-button.md).
