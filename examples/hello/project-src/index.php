<?php
// The simplest thing: a plain script that runs top to bottom and stops.
// Copy it to the microSD as index.php, reset, and watch the serial output.

echo "Hello from PHP " . PHP_VERSION . " on an ESP32-P4!\n";
echo "2 ** 16 = " . (2 ** 16) . "\n";
echo "memory in use: " . memory_get_usage() . " bytes\n";

foreach (['world', 'chip', 'microcontroller'] as $who) {
    echo "hello, $who\n";
}
