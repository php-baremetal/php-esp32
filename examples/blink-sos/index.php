<?php
// Blinks "SOS" in Morse code on an LED, forever.
// LED + ~330 ohm resistor between GPIO2 and GND. Copy to the microSD as index.php.

define('LED', 2);
const UNIT = 200;   // milliseconds; a dot is one unit, a dash is three

function pulse(int $units): void {
    gpio_write(LED, 1);
    delay(UNIT * $units);
    gpio_write(LED, 0);
    delay(UNIT);            // gap between symbols
}

function setup(): void {
    gpio_mode(LED, GPIO_OUTPUT);
    echo "blinking SOS on GPIO " . LED . "\n";
}

function loop(int $tick): void {
    foreach ([1, 1, 1, 3, 3, 3, 1, 1, 1] as $units) {   // S O S
        pulse($units);
    }
    delay(UNIT * 6);        // longer gap before repeating
}
