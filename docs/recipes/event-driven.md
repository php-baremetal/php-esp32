---
eyebrow: 'Docs · Recipes'
lede:    'The event-driven model: register listeners once, start sources (a timer, a GPIO interrupt), then let a resident reactor deliver typed events to your PHP — sleeping between events, no busy-wait, state kept in ordinary variables.'
see_also:
  - { href: '../getting-started/execution-models.md', meta: '6 min' }
  - { href: './http-and-websocket.md', meta: '8 min' }
  - { href: './read-a-button.md', meta: '5 min' }
prev: { label: 'Execution models', href: '../getting-started/execution-models.md' }
next: { label: 'HTTP & WebSocket', href: './http-and-websocket.md' }
---

# Event-driven basics

In the `event-driven` model there is no `loop()`. Your script runs **once** to register listeners and
start *sources*, then a reactor blocks on an event queue and delivers each event to its listeners. The
engine stays resident, so a variable captured by a listener persists across events.

Select the model in `php-esp32.config.toml`:

```toml
type = "event-driven"
```

## Events and listeners

An **event** is a PHP object — any subclass of `Baremetal\Event`. You subscribe with
`Events::listen()` and dispatch with `Events::now()` (inline) or `Events::dispatch()` (deferred).

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
use Baremetal\Event;
use Baremetal\Events;

final class Tick extends Event {}

$ticks = 0;

Events::listen(Tick::class, function (Tick $e) use (&$ticks): void {
    printf("tick %d\n", ++$ticks);
});

every(1000, Tick::class);   // a source: emit a Tick once a second

echo "listeners registered; the reactor now runs\n";
```

- **`Events::listen(Class::class, callable)`** — register a listener. Only during setup: after the
  script returns, the listener table is frozen and `listen()` throws.
- **`Events::now($event)`** — deliver right now, inline.
- **`Events::dispatch($event)`** — deliver after the current handler returns (breadth-first).
- A listener returning **`false`** stops propagation to later listeners of the same event.
- Match is by **exact class**. `$event->device` is the emitter (or `null` for a plain PHP event).

## Sources

A *source* is what feeds the reactor. Two are built in as global functions (call them during setup):

- **`every(int $ms, string $eventClass)`** — emit an instance of `$eventClass` every `$ms`.
- **`watch_gpio(int $pin, string $eventClass)`** — emit on a debounced falling edge (a button).

```php
final class BootPressed extends Baremetal\Event {}

Events::listen(BootPressed::class, fn () => print("button!\n"));
watch_gpio(0, BootPressed::class);   // GPIO0 = the BOOT button on many boards
```

Sources run below your code: the timer and the GPIO interrupt hand a typed event to the reactor, which
delivers it on the PHP thread. Between events the reactor sleeps, so the CPU is idle without you writing
a delay.

## A sensor as a source

A polled I²C sensor can be a source too. `poll()` samples it on core 1; with `event:` it raises a
`SamplesReady` event on each new sample, carrying the sensor as `$e->device`:

```php
use Baremetal\I2c\{Bus, Driver\Qmi8658};
use Baremetal\Sensor\Imu\SamplesReady;

$imu = new Qmi8658(new Bus(sda: 11, scl: 10));

Events::listen(SamplesReady::class, function (SamplesReady $e): void {
    ['accel' => [$x, $y, $z]] = $e->device->decode($e->device->sample());
    printf("accel %+.2f %+.2f %+.2f g\n", $x, $y, $z);
});

$imu->poll(hz: 20, depth: 16, event: SamplesReady::class);
```

The sampling runs on core 1; the handler reads the latest sample from the ring — no bus traffic, no
polling in your code. See [imu-ws-stream](../../examples/imu-ws-stream/) for the same idea streamed to a
browser.

## Why it matters

Because the reactor blocks instead of looping, an event-driven device does nothing — and draws little —
until something happens: a timer fires, a pin changes, a request arrives. That is also the foundation for
serving HTTP and WebSocket on the same reactor ([HTTP & WebSocket](./http-and-websocket.md)).
