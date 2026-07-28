<?php
// setup()/loop() sketch, Arduino-style. setup() runs once; loop($tick) repeats.
// The C side owns the loop (watchdog + memory housekeeping); PHP provides these two.
//
// Wire an LED + resistor between GPIO2 and GND (or change LED below).
// The GPIO drives 3.3V, so a red LED (~330 ohm) is a safe bet; a blue/white LED
// (higher forward voltage) can be too dim to see at 3.3V.
// Copy this to the microSD as /index.php, reset, and the LED blinks forever.

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
