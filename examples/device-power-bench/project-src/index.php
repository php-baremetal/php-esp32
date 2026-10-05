<?php
// Power bench (event-driven) on the Button source.
//   Click (short press)      -> screen off + light sleep (press BOOT to wake).
//   Held  (hold 2s)          -> deep sleep (press BOOT to wake, full reboot).
// The Button source owns debounce/press/hold/release timing in C, so there is no manual polling here.
// A time guard drops the boot/wake-edge clicks. Read each state's current on the CT-3.

use Baremetal\Event;
use Baremetal\Events;
use Baremetal\Spi\Bus;
use Baremetal\Spi\Driver\St77916;

const BTN = 0;    // BOOT button, active low
const BL  = 5;    // LCD backlight

final class Clicked extends Event {}
final class Held extends Event {}

$bus = new Bus(sclk: 40, mosi: 46, miso: 45, data2: 42, data3: 41);
$lcd = new St77916($bus, cs: 21, rst: 3, bl: -1, width: 360, height: 360);

gpio_mode(BL, GPIO_OUTPUT);

function ms(): float
{
    return microtime(true) * 1000.0;
}

function red_on(): void
{
    $lcd = $GLOBALS['lcd'];
    $lcd->fill($lcd->rgb(255, 0, 0));
    gpio_write(BL, 1);
}

$GLOBALS['guard'] = ms() + 2000;   // ignore button events until then (boot-time edges)

if (wake_cause() !== 0) {
    echo "woke from DEEP SLEEP (cause=" . wake_cause() . ")\n";
}
red_on();
echo "[ACTIVE] screen RED -- baseline. BOOT: click = sleep, hold 2s = deep sleep.\n";

Events::listen(Clicked::class, function (): void {
    if (ms() < $GLOBALS['guard']) {
        return;   // boot edge, or the press that just woke us
    }
    echo "[ASLEEP] screen off + light sleep -- read current; press BOOT to wake.\n";
    gpio_write(BL, 0);
    button_sampling(false);        // stop the sampler so the chip can light-sleep
    light_sleep(BTN);              // reactor sleeps here until BOOT goes low
    $GLOBALS['lcd']->wake();       // re-init the panel (light sleep disturbed the QSPI lines)
    red_on();
    $GLOBALS['guard'] = ms() + 600;   // ignore the wake press's click
    button_sampling(true);
    echo "[ACTIVE] woke -> screen RED.\n";
});

Events::listen(Held::class, function (): void {
    if (ms() < $GLOBALS['guard']) {
        return;
    }
    echo "[DEEP SLEEP] read current; press BOOT to wake (reboot).\n";
    gpio_write(BL, 0);
    while (gpio_read(BTN) === 0) {   // release first, else ext1 (GPIO0 low) wakes instantly
        usleep(5000);
    }
    deep_sleep(BTN);               // never returns
});

watch_button(BTN, ['click' => Clicked::class, 'held' => Held::class], ['holdMs' => 2000, 'debounceMs' => 30]);
