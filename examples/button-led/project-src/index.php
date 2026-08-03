<?php
// Reads a push button and mirrors it to an LED.
//   LED    + ~330 ohm resistor between GPIO2 and GND
//   button between GPIO4 and GND
// gpio_mode() resets the pin, which leaves the internal pull-up on, so the input
// reads 1 when idle and 0 when the button pulls it to ground. Copy to the microSD
// as index.php.

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
