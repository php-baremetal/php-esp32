<?php
// ESP32-S3 onboard RGB LED: a slow, continuous rainbow, in PHP.
//
// Needs the `s3_onboard_rgb` extension (ESP32-S3 only) and the board's WS2812 LED; the
// data pin comes from [extensions.s3_onboard_rgb] pin in the config (default 48).

const BRIGHT = 10;   // 0..255 value -- kept very low on purpose; the WS2812 is dazzlingly bright

function setup(): void
{
    if (!s3_onboard_rgb_available()) {
        echo "s3_onboard_rgb not built -- enable [extensions.s3_onboard_rgb] on an ESP32-S3 board\n";
        return;
    }
    echo "s3_onboard_rgb rainbow on GPIO " . S3_ONBOARD_RGB_PIN . " -- PHP " . PHP_VERSION . "\n";
}

function loop(int $tick): void
{
    // One smooth hue sweep per call; loop() is re-entered forever, so the rainbow never stops.
    for ($h = 0; $h < 360; $h += 2) {   // 180 steps
        s3_onboard_rgb_hsv($h, 255, BRIGHT);
        delay(25);                      // ~4.5 s per full cycle
    }
}
