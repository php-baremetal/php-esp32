---
eyebrow: 'Docs · Recipes'
lede:    'Read the QMI8658 6-axis IMU (accelerometer + gyroscope + on-chip temperature) over I²C: direct reads, background polling on core 1, or as a SamplesReady event source in the event-driven model.'
see_also:
  - { href: './event-driven.md', meta: '8 min' }
  - { href: './http-and-websocket.md', meta: '8 min' }
  - { href: '../getting-started/execution-models.md', meta: '6 min' }
prev: { label: 'Read a button', href: './read-a-button.md' }
next: { label: 'Drive an SSD1306 OLED', href: './ssd1306-oled.md' }
---

# Read the QMI8658 IMU

The **QMI8658** is a 6-axis IMU — a 3-axis accelerometer and a 3-axis gyroscope, plus an on-chip
temperature sensor — on the I²C bus. It ships as a driver in the `i2c` extension, exposed as
`Baremetal\I2c\Driver\Qmi8658`.

## Enable it

```toml
[extensions.i2c]
enabled = true
drivers = ["qmi8658"]
```

## Wire it up

It sits on an I²C bus; you pass the pins when you make the bus. On the ESP32-S3-Touch boards the IMU is on
`SDA=11`, `SCL=10`:

```php
use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Qmi8658;

$imu = new Qmi8658(new Bus(sda: 11, scl: 10));
```

`new Qmi8658(Bus $bus, int $address = auto, int $hz = 400000)` — the address defaults to the chip's
(0x6B/0x6A) and the bus runs at 400 kHz. Other devices can share the same bus (different addresses).

## Direct reads

The simplest use — read the current values whenever you like (each call touches the bus):

- `accel(): array` → `[x, y, z]` in **g**
- `gyro(): array` → `[x, y, z]` in **°/s** (dps)
- `temp(): float` → the on-chip temperature in **°C**

<!-- @code-block language="php" label="project-src/index.php (init-loop)" -->
```php
<?php
use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Qmi8658;

$imu = new Qmi8658(new Bus(sda: 11, scl: 10));

function loop(int $tick): void
{
    global $imu;
    [$ax, $ay, $az] = $imu->accel();
    [$gx, $gy, $gz] = $imu->gyro();
    printf("accel %+.2f %+.2f %+.2f g   gyro %+.1f %+.1f %+.1f dps   %.1f C\n",
        $ax, $ay, $az, $gx, $gy, $gz, $imu->temp());
    delay(200);
}
```

At rest one accel axis reads ≈ ±1 g (gravity) and the gyro reads ≈ 0.

## Polling on core 1

For a steady, jitter-free sample rate, hand the sampling to the executor on **core 1**: it reads the
sensor at a fixed rate into a ring buffer, independent of how slow your loop is. Your code then reads
snapshots from the ring — no bus traffic, never blocked by a busy loop.

- `poll(int $hz, int $depth = 64, ?string $event = null): void` — start the core-1 poller.
- `sample(): ?string` — the most recent raw sample (12 bytes), or `null` if none yet.
- `drain(): array` — every raw sample taken since the last call (oldest first).
- `decode(string $raw): array` → `['accel' => [x,y,z], 'gyro' => [x,y,z]]`.

<!-- @code-block language="php" label="project-src/index.php (init-loop + core-1 poll)" -->
```php
<?php
use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Qmi8658;

$imu = new Qmi8658(new Bus(sda: 11, scl: 10));
$imu->poll(hz: 100, depth: 64);          // sampling now runs on core 1

function loop(int $tick): void
{
    global $imu;
    $batch = $imu->drain();              // ~20 raw samples at a 5 Hz loop, none lost
    foreach ($batch as $raw) {
        ['accel' => [$ax, $ay, $az]] = $imu->decode($raw);
        // ... process every sample ...
    }
    delay(200);
}
```

See the [`imu-poll-core1`](../../examples/imu-poll-core1/) example.

## As an event source (event-driven)

Add `event:` and each new core-1 sample raises a `Baremetal\Sensor\Imu\SamplesReady` event on the
reactor, carrying the sensor as `$e->device`. No loop, no polling in your code — you react to fresh data:

<!-- @code-block language="php" label="project-src/index.php (event-driven)" -->
```php
<?php
use Baremetal\Events;
use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Qmi8658;
use Baremetal\Sensor\Imu\SamplesReady;

$imu = new Qmi8658(new Bus(sda: 11, scl: 10));

Events::listen(SamplesReady::class, function (SamplesReady $e): void {
    ['accel' => [$x, $y, $z]] = $e->device->decode($e->device->sample());
    printf("accel %+.2f %+.2f %+.2f g\n", $x, $y, $z);
});

$imu->poll(hz: 30, depth: 16, event: SamplesReady::class);
```

Keep the rate modest (each event shares the reactor queue with anything else — HTTP, timers): ~30 Hz is
plenty for a UI and leaves the queue room. The [`imu-ws-stream`](../../examples/imu-ws-stream/) example
streams accel + gyro + temperature to a browser over WebSocket this way.

## The `Imu` capability

The driver implements `Baremetal\Sensor\Imu`, a marker interface. Code that only needs "an IMU" can accept
any chip that implements it, so swapping the QMI8658 for another IMU driver later doesn't change the
consumer:

```php
if ($imu instanceof Baremetal\Sensor\Imu) {
    // it's an IMU, whatever the chip
}
```

## Notes

- `accel()`/`gyro()`/`temp()` each read the bus on call; while a core-1 poller is running they still work
  (the bus is locked per transaction), but prefer `sample()`/`drain()` from the ring to avoid extra I/O.
- Temperature is a separate register (not part of the polled block), so read it with `temp()` when you
  need it rather than every sample.
- The accelerometer is ±4 g and the gyroscope ±512 °/s by default.
```
