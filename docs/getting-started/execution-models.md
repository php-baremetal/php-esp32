---
eyebrow: 'Docs · Getting started'
lede:    'Three ways the firmware runs your PHP: init-loop (setup/loop), web-server (fresh PHP per HTTP request), and event-driven (a resident reactor that blocks on an event queue). Pick one with `type` in the project config.'
see_also:
  - { href: './quick-start.md', meta: '10 min' }
  - { href: '../recipes/event-driven.md', meta: '8 min' }
  - { href: '../recipes/http-and-websocket.md', meta: '8 min' }
prev: { label: 'Architecture', href: './architecture.md' }
next: { label: 'Event-driven basics', href: '../recipes/event-driven.md' }
---

# Execution models

The firmware can run your PHP in three different shapes. You choose one per project with `type` in
`php-esp32.config.toml`; the rest of your code is written to match. They are peers — the engine, the
extensions and the storage are the same underneath.

| `type` | Shape | Your entry point | State between "runs" |
|---|---|---|---|
| `init-loop` | `setup()` once, then `loop($tick)` forever | a script with `setup`/`loop` | kept (one long-lived run) |
| `web-server` | an HTTP server runs PHP **fresh per request** | a front controller (`index.php`) | **torn down** each request (shared-nothing) |
| `event-driven` | run once to register listeners, then a reactor delivers events | a setup script | kept (one resident run) |

## `init-loop` — the Arduino shape

The default. Your script defines `setup()` (run once) and `loop(int $tick)` (called forever). Good for
a blink, a sensor read, a bit of local logic. `delay($ms)` paces the loop.

<!-- @code-block language="php" label="init-loop" -->
```php
function setup(): void { gpio_mode(2, OUTPUT); }
function loop(int $tick): void { gpio_write(2, $tick % 2); delay(500); }
```
<!-- @endcode-block -->

A script with no `loop()` just runs once and stops — fine for a one-shot.

## `web-server` — fresh PHP per HTTP request

An HTTP server (in the firmware) sits in front and runs your entry script **once per request**,
shared-nothing, exactly like PHP behind Apache/nginx. The request becomes `$_SERVER`/`$_GET`/`$_POST`
and whatever you echo is the response. This is the model that runs stock Laravel. See
[Serve a web page](../recipes/web-page.md). State that must survive a request goes below userland
(the [in-RAM store](../storage/in-ram-store.md), or a run-once `[web-server] init` script).

## `event-driven` — a resident reactor

The script runs **once** to register listeners and start *sources* (a timer, a GPIO interrupt, an HTTP
or WebSocket endpoint), then a reactor **blocks on an event queue** and delivers each event to its
listeners — sleeping in between, so the CPU idles with no busy-wait. The engine stays resident, so
ordinary PHP variables persist between events (no per-request teardown).

<!-- @code-block language="php" label="event-driven" -->
```php
use Baremetal\Events;
final class Tick extends Baremetal\Event {}

$n = 0;
Events::listen(Tick::class, function (Tick $e) use (&$n) { printf("tick %d\n", ++$n); });
every(1000, Tick::class);          // a source: one Tick per second
```
<!-- @endcode-block -->

It composes the other two ideas: add an `http` source and you serve HTTP *and* react to hardware and
WebSocket messages on one resident reactor. This is the model for a device that reacts — a button, a
sensor, a live web UI — rather than one that loops. Start at [Event-driven basics](../recipes/event-driven.md),
then [HTTP & WebSocket](../recipes/http-and-websocket.md).

## Choosing

- Blinking, a local sensor loop, a one-shot → **`init-loop`**.
- A browsable site or REST app, especially a framework → **`web-server`**.
- A reactive device (buttons, sensors, live push, low-power) → **`event-driven`**.

Set it in the project config:

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
type = "event-driven"
```
<!-- @endcode-block -->
