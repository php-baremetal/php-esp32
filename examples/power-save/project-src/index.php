<?php
// power_save verification: the build turns on esp_pm automatic light sleep, so the chip sleeps
// whenever the reactor is idle between events. A heartbeat every 10s is the only scheduled work,
// so the board sits in light sleep almost the whole time -- watch the current drop on the CT-3.
//
// power_hold()/power_release() bracket a busy stretch: while held, light sleep is forbidden and the
// current stays high; after release it drops back. The reactor also auto-releases at the end of each
// handler, so a forgotten release can't pin the chip awake forever.

use Baremetal\Events;

final class Tick extends Baremetal\Event {}

Events::listen(Tick::class, function () {
    static $n = 0;
    $n++;
    echo "tick $n -- idle (light sleep between beats)\n";

    // every 3rd beat, stay awake ~2s to show the current rise under power_hold()
    if ($n % 3 === 0) {
        echo "  power_hold: busy ~2s (no sleep)\n";
        power_hold();
        $end = microtime(true) + 2.0;
        while (microtime(true) < $end) { /* spin */ }
        power_release();
        echo "  power_release: back to idle\n";
    }
});

every(10000, Tick::class);
echo "power-save demo up: heartbeat every 10s, auto light sleep in between\n";
