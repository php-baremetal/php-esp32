# imu-events

The executor feeding the reactor: **core 1 polls the IMU, core 0 reacts.** This is the event-driven
counterpart of [`imu-poll-core1`](../imu-poll-core1/) — instead of a `loop()` draining the ring, a
`SamplesReady` event wakes a listener when new data arrives, and the reactor sleeps in between.

```php
use Baremetal\Events;
use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Qmi8658;
use Baremetal\Sensor\Imu\SamplesReady;

$imu = new Qmi8658(new Bus(sda: 11, scl: 10));

Events::listen(SamplesReady::class, function (SamplesReady $e): void {
    foreach ($e->device->drain() as $raw) {          // the ring, no bus traffic
        ['accel' => [$ax, $ay, $az]] = $e->device->decode($raw);
        printf("accel = [% .2f % .2f % .2f] g\n", $ax, $ay, $az);
    }
});

$imu->poll(hz: 10, depth: 32, event: SamplesReady::class);   // core-1 poll + emit
```

## How it works

`poll(hz, depth, event: …)` hands the sampling to the core-1 executor and, because `event:` is set,
asks it to emit that event on **each new sample**. The event crosses to the reactor on core 0, which
delivers it as a typed object:

- `$e` is an instance of the class you named (here `SamplesReady`).
- `$e->device` is the sensor that produced it — the same `$imu` handle. It is how a handler tells two
  identical sensors apart, and how it reads the data: `$e->device->drain()` / `->decode()` touch the
  ring only, never the bus.

`SamplesReady` is a **shared contract** (`Baremetal\Sensor\Imu\SamplesReady`, a `Baremetal\Event`
subclass registered by the i2c extension): the same handler works for any polled IMU, whatever chip is
underneath.

`poll()` without `event:` is unchanged — it fills the ring for `sample()`/`drain()` and emits nothing,
so the init-loop style ([`imu-poll-core1`](../imu-poll-core1/)) still works.

## Run

```sh
phpflash flash
phpflash monitor      # a line per sample: accel ~[0 0 1] g at rest, reacting to SamplesReady
```
