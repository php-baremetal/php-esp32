<?php
// ESP32-S3 onboard RGB LED: a slow, continuous rainbow, in PHP.
//
// Needs the `led` extension with the ws2812 driver. The onboard LED is a single WS2812 in RGB byte
// order (standard strips are GRB), so pass Ws2812::RGB. The data pin is 48 on the S3-Zero (38 on some
// S3 boards) -- change it to match yours.

use Baremetal\Led\Driver\Ws2812;

const PIN    = 48;
const BRIGHT = 10;   // 0..255 value -- kept very low on purpose; the WS2812 is dazzlingly bright

$led = null;

function setup(): void
{
    global $led;
    $led = new Ws2812(PIN, 1, Ws2812::RGB);
    echo "ws2812 rainbow on GPIO " . PIN . " -- PHP " . PHP_VERSION . "\n";
}

function loop(int $tick): void
{
    global $led;
    // One smooth hue sweep per call; loop() is re-entered forever, so the rainbow never stops.
    for ($h = 0; $h < 360; $h += 2) {   // 180 steps
        $led->hsv(0, $h, 255, BRIGHT);
        $led->show();
        delay(25);                      // ~4.5 s per full cycle
    }
}
