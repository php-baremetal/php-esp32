<?php
/**
 * The IMU sampled at 100 Hz on core 1 while loop() runs at 5 Hz.
 *
 * $imu->poll(hz, depth) hands the sampling to the executor task on core 1: it reads the sensor at a
 * fixed rate into a ring buffer, independent of how slow the PHP loop is. loop() then drain()s every
 * sample taken since last time -- so a 5 Hz loop still sees ~20 samples per tick, none lost. sample()
 * and drain() touch the ring only, never the bus.
 */

use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Qmi8658;

$imu = new Qmi8658(new Bus(sda: 11, scl: 10));
$imu->poll(hz: 100, depth: 64);      // sampling now runs on core 1

function loop(int $tick): void
{
    global $imu;

    $batch = $imu->drain();          // every raw sample since the last tick
    $latest = $imu->sample();        // the most recent raw sample (or null)

    if ($latest === null) {
        echo "no samples yet\n";
    } else {
        ['accel' => [$ax, $ay, $az]] = $imu->decode($latest);
        printf("core-1 poller: drained %2d samples  latest accel = [% .2f % .2f % .2f] g\n",
            count($batch), $ax, $ay, $az);
    }

    delay(200);                      // 5 Hz loop
}
