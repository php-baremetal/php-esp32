<?php
/**
 * The event-driven execution model: no loop(). The script runs once to register listeners and start
 * sources, then the reactor blocks on the event queue and delivers each event to its listeners --
 * sleeping between events, so the CPU idles (no busy-wait).
 *
 * Sources are C producers: every() is a timer, watch_gpio() a debounced GPIO interrupt. Each emits a
 * typed event; the handler receives the object.
 */

use Baremetal\Event;
use Baremetal\Events;

final class Tick extends Event {}
final class BootPressed extends Event {}

$ticks = 0;

Events::listen(Tick::class, function (Tick $e) use (&$ticks): void {
    printf("tick %d  (a Tick delivered by the reactor)\n", ++$ticks);
});

Events::listen(BootPressed::class, function (BootPressed $e): void {
    echo "  BOOT button pressed!\n";
});

every(1000, Tick::class);          // a Tick every second (timer source)
watch_gpio(0, BootPressed::class); // GPIO0 = the BOOT button (falling edge, debounced)

echo "event-driven: registered listeners + sources; the reactor now blocks on the queue\n";
