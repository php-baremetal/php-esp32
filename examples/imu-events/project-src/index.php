<?php
/**
 * The executor feeding the reactor: core 1 polls the IMU, core 0 reacts.
 *
 * $imu->poll(hz, depth, event: SamplesReady::class) hands the sampling to the core-1 executor AND asks
 * it to emit a SamplesReady event on each new sample. The event travels to the reactor on core 0, which
 * delivers it as a typed object -- $e->device is the IMU that produced it. The handler reads the ring
 * (drain(), no bus traffic) and decodes; between events the reactor sleeps.
 *
 * SamplesReady is the shared sensor contract: the same handler works for any polled IMU, whatever chip.
 */

use Baremetal\Events;
use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Qmi8658;
use Baremetal\Sensor\Imu\SamplesReady;

$imu = new Qmi8658(new Bus(sda: 11, scl: 10));

$seen = 0;

Events::listen(SamplesReady::class, function (SamplesReady $e) use (&$seen): void {
    foreach ($e->device->drain() as $raw) {          // every sample since the last event, from the ring
        ['accel' => [$ax, $ay, $az]] = $e->device->decode($raw);
        printf("samples-ready #%d  accel = [% .2f % .2f % .2f] g\n", ++$seen, $ax, $ay, $az);
    }
});

$imu->poll(hz: 10, depth: 32, event: SamplesReady::class);   // core-1 poll + emit, 10 Hz

echo "imu-events: polling on core 1, reacting to SamplesReady on core 0\n";
