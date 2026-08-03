# led-blink

Three lines of PHP that blink an LED on a microcontroller. This is the example that
ties everything together and shows the project actually works.

![The LED blinking](display.gif)

## What it does

`index.php` defines two functions, `setup()` and `loop()`, Arduino-style. The board
runs the script once (which defines the functions), then calls `setup()` once and
`loop($tick)` over and over. `setup()` makes GPIO2 an output; `loop()` toggles the LED
each tick with half a second between changes.

```php
define('LED', 2);

function setup(): void {
    gpio_mode(LED, GPIO_OUTPUT);
    echo "PHP " . PHP_VERSION . " on ESP32-P4\n";
}

function loop(int $tick): void {
    gpio_write(LED, $tick % 2);
    delay(500);
}
```

## What it demonstrates

This example is the proof that the things that matter actually hold up, on real
silicon, all at once:

- **It's real PHP running on the chip.** The log says `PHP 8.3.32 on ESP32-P4`: it's the
  official interpreter, not a simulation. The script is read from a microSD, compiled to
  opcodes, and executed by the Zend engine on the RISC-V core.
- **PHP drives the hardware.** `gpio_mode`, `gpio_write` and `delay` are PHP calls that
  move a physical pin: the LED in the video blinks because PHP code tells it to.
- **The setup/loop model works.** `setup()` runs once, `loop()` repeats, with the loop
  driven from C (which keeps the watchdog happy and does memory housekeeping).
- **Memory is stable.** In the log the free heap stays put around 32.7 MB tick after
  tick: the engine runs continuously without leaking.

## The output

Excerpt from [`monitor.txt`](monitor.txt) (the full serial log):

```
php-esp32: starting PHP runtime
php-esp32: microSD mounted at /sdcard
php-esp32: php_embed_init()...
PHP 8.3.32 on ESP32-P4
setup: blinking an LED on GPIO 2 from PHP 8.3.32
php-esp32: entering loop()
tick 0
php-esp32: tick 0 -- heap free: 32725947 bytes
tick 10
tick 20
...
```

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

To run from a microSD instead, copy `project-src/index.php` to the card root and press
reset.

Wiring: an LED with a series resistor between GPIO2 and GND. The GPIO drives **3.3V**,
not 5V, so a red LED with ~330 ohm is clearly visible; a blue or white LED (higher
forward voltage) can be too dim to see at 3.3V.
