<?php
/**
 * The Events facade: typed event classes, listeners registered in setup(), now() vs dispatch().
 *
 * An event is a subclass of Baremetal\Event. Events::listen() maps a class to listeners (only in
 * setup() -- the table freezes afterwards). Events::now() delivers inline; Events::dispatch() delivers
 * after the current handler returns. `return false` from a listener stops propagation.
 */

use Baremetal\Event;
use Baremetal\Events;

final class Tick extends Event
{
    public function __construct(public int $n) {}
}

final class Ping extends Event {}

function setup(): void
{
    Events::listen(Tick::class, function (Tick $e): void {
        printf("  Tick listener A: n=%d\n", $e->n);
    });
    Events::listen(Tick::class, function (Tick $e): void {
        echo "  Tick listener B saw it too\n";
    });
    Events::listen(Ping::class, function (Ping $e): void {
        echo "  Ping! (delivered inline by now())\n";
    });
    echo "listeners registered in setup()\n";
}

function loop(int $tick): void
{
    printf("tick %d:\n", $tick);

    if ($tick % 3 === 0) {
        Events::now(new Ping());          // inline, synchronous
    }
    Events::dispatch(new Tick($tick));    // deferred: runs after this handler returns

    delay(1000);
}
