<?php
// CORE1 ownership: the core-1 executor owns the wire; every transaction here is queued to it.

use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Qmi8658;

$bus = new Bus(sda: 11, scl: 10, owner: Bus::CORE1);
$imu = new Qmi8658($bus);   // init writes are queued to core 1

echo "scan (CORE1):\n";     // the sweep runs on core 1
foreach ($bus->scan() as $addr => $info) {
    printf("  0x%02X  %s\n", $addr, $info['driver'] ?? '(no driver)');
}

function loop(): void
{
    global $imu;   // loop() is a named function; the top-level $imu isn't in its scope

    [$ax, $ay, $az] = $imu->accel();   // read routed through the core-1 queue
    printf("accel (CORE1) = [% .2f % .2f % .2f] g\n", $ax, $ay, $az);
    usleep(500000);
}
