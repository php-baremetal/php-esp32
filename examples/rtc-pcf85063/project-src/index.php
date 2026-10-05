<?php
// PCF85063 RTC: set the time once (if unset), then read it every second.

use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Pcf85063;

$bus = new Bus(sda: 11, scl: 10);   // onboard I2C
$rtc = new Pcf85063($bus);

if (!$rtc->now()['valid']) {
    $rtc->set(2026, 10, 4, 12, 0, 0);   // only on a fresh (power-lost) RTC
    echo "RTC was unset -> seeded 2026-10-04 12:00:00\n";
}

function loop(): void
{
    global $rtc;

    $t = $rtc->now();
    printf("%04d-%02d-%02d %02d:%02d:%02d  valid=%s\n",
        $t['year'], $t['month'], $t['day'], $t['hour'], $t['minute'], $t['second'],
        $t['valid'] ? 'yes' : 'no');
    sleep(1);
}
