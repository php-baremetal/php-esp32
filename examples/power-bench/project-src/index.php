<?php
// power-bench (S3-Zero): an automated power bench. Cycles three states, 5 s each, with a clear serial
// marker per state, so each one's current can be read on the meter. Event-driven; deep sleep reboots,
// so the cycle repeats on its own.
//   ACTIVE      - CPU awake (busy)
//   LIGHT SLEEP - esp_light_sleep, timer wake (PSRAM retained)
//   DEEP SLEEP  - esp_deep_sleep, timer wake (full reboot)

use Baremetal\Event;
use Baremetal\Events;

const SECS = 5;

final class Cycle extends Event {}

echo "power-bench (S3-Zero): active / light sleep / deep sleep, 5 s each.\n";
if (wake_cause() !== 0) {
    echo "(back from deep sleep -> new cycle)\n";
}

Events::listen(Cycle::class, function (): void {
    echo "[ACTIVE] " . SECS . "s -- read current\n";
    $t = microtime(true);
    while (microtime(true) - $t < SECS) {
        // CPU awake and busy
    }

    echo "[LIGHT SLEEP] " . SECS . "s -- read current\n";
    light_sleep(-1, SECS * 1000);      // timer-only wake

    echo "[DEEP SLEEP] " . SECS . "s -- read current (then reboot)\n";
    usleep(200000);
    deep_sleep(-1, SECS * 1000);       // timer-only wake -> full reboot -> cycle repeats
});

Events::now(new Cycle());
