---
eyebrow: 'Docs · Recipes'
lede: 'Poll a push button from PHP and mirror its state to an LED — the read-a-pin, decide, drive-a-pin loop that most microcontroller programs are built from.'
see_also:
  - href: ./blink-an-led.md
    meta: 'Recipes'
    label: 'Blink an LED'
  - href: ./ssd1306-oled.md
    meta: 'Recipes'
    label: 'Drive an SSD1306 OLED'
  - href: ../getting-started/quick-start.md
    meta: 'Getting started'
    label: 'Quick start'
prev:
  label: 'Blink an LED'
  href: ./blink-an-led.md
next:
  label: 'Drive an SSD1306 OLED'
  href: ./ssd1306-oled.md
---

# Read a button

Blinking an LED only drives an output. This recipe reads an input too: PHP polls a push button and lights an LED while the button is held down. Read a pin, decide, drive another pin, repeat — the shape of most microcontroller programs, written entirely in PHP.

## Goal

Mirror a physical button to an LED. When the button is pressed the LED turns on; when it is released the LED turns off.

## What you need

- An ESP32-P4 board running php-esp32.
- A push button wired between **GPIO4** and **GND**.
- A red LED with a series resistor (~330 ohm) between **GPIO2** and **GND**.

No external resistor is needed on the button: resetting the pin leaves its internal pull-up on, so the input idles high and drops to ground only while the button is pressed. The GPIO drives **3.3V**, not 5V, so a red LED is the safe choice — a blue or white LED can be too dim to see at 3.3V.

## The code

`setup()` makes GPIO2 an output and GPIO4 an input. `loop()` reads the button every 20 ms and writes its state to the LED.

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
// Reads a push button and mirrors it to an LED.
//   LED    + ~330 ohm resistor between GPIO2 and GND
//   button between GPIO4 and GND

define('LED', 2);
define('BUTTON', 4);

function setup(): void {
    gpio_mode(LED, GPIO_OUTPUT);
    gpio_mode(BUTTON, GPIO_INPUT);
    echo "mirroring the button (GPIO " . BUTTON . ") to the LED (GPIO " . LED . ")\n";
}

function loop(int $tick): void {
    $pressed = gpio_read(BUTTON) === 0;   // pulled low when pressed
    gpio_write(LED, $pressed ? 1 : 0);
    delay(20);
}
```
<!-- @endcode-block -->

`gpio_read()` brings the pin's physical state back into the script and returns `0` or `1`. Because the button pulls GPIO4 to ground, a press reads `0` — hence the `=== 0` test.

<!-- @callout variant="note" title="Internal pull-up and debouncing" -->
`gpio_mode(BUTTON, GPIO_INPUT)` resets the pin, which leaves its internal pull-up enabled. The input therefore reads `1` when the button is idle and `0` when the button connects the pin to ground — no external pull-up resistor required. Polling every 20 ms feels instant while smoothing over most contact bounce; a plain on/off mirror like this one needs no extra debounce logic. If you act on the transition rather than the level (counting presses, toggling on each click), remember the last state and only react when it changes.
<!-- @endcallout -->

## Config

A minimal `init-loop` project running its source from the microSD:

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
name = "button-led"
storage_type = "microsd"   # where the PHP source lives
type = "init-loop"         # execution model

[board]
target = "esp32-p4-pico"
port   = ""                # empty = autodetect at flash time

[php]
src   = "project-src"      # PHP source folder
entry = "index.php"        # entry file within src
```
<!-- @endcode-block -->

## Build & flash

<!-- @code-block language="bash" label="Build, flash, and watch the serial log" -->
```bash
phpflash build && phpflash flash && phpflash monitor
```
<!-- @endcode-block -->

To run from a microSD instead of rebuilding, copy `project-src/index.php` to the card root and press reset.

## What you'll see

Press the button and the LED lights; release it and the LED goes dark. The serial log prints the setup line once, then ticks steadily with a flat free-heap figure across thousands of iterations:

<!-- @code-block language="text" label="Serial output" -->
```
PHP 8.3.32 on ESP32-P4
--- /sdcard/index.php ---
mirroring the button (GPIO 4) to the LED (GPIO 2)
php-esp32: entering loop()
php-esp32: tick 0 -- heap free: 32694175 bytes
php-esp32: tick 256 -- heap free: 32694175 bytes
php-esp32: tick 512 -- heap free: 32694175 bytes
```
<!-- @endcode-block -->

The full example lives in [`examples/button-led/`](../../examples/button-led/).
