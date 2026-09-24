# event-driven-hello

The `event-driven` execution model: no `loop()`. The script runs once to register listeners and start
sources, then a reactor blocks on the event queue and delivers each event to its listeners — sleeping
between events, so the CPU idles (no busy-wait).

Sources are C producers that emit typed events onto the queue:

- `every(ms, class)` — a timer; emits an instance of `class` every `ms`.
- `watch_gpio(pin, class)` — a debounced falling-edge interrupt; emits `class` on each press.

```php
use Baremetal\Event;
use Baremetal\Events;

final class Tick extends Event {}
final class BootPressed extends Event {}

$ticks = 0;

Events::listen(Tick::class, function (Tick $e) use (&$ticks): void {
    printf("tick %d\n", ++$ticks);
});

Events::listen(BootPressed::class, function (BootPressed $e): void {
    echo "  BOOT button pressed!\n";
});

every(1000, Tick::class);          // a Tick every second (timer source)
watch_gpio(0, BootPressed::class); // GPIO0 = the BOOT button (falling edge, debounced)
```

After the top-level script returns, the listener table is frozen and the reactor takes over. Each
timer fire and each button press is delivered on the PHP thread as its event object — state captured
by the listener (here `$ticks`) persists across deliveries.

## Config

`type = "event-driven"` in `php-esp32.config.toml` selects this model (no HTTP server, no init-loop).

## Run

```sh
phpflash flash
phpflash monitor      # "entering event loop", then one tick per second; press BOOT for a line
```
