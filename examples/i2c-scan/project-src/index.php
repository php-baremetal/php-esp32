<?php
/**
 * I2C bus scan -- lists every device that answers on the wire.
 *
 * Baremetal\I2c\Bus is an idempotent handle onto an I2C master bus: same pins, same bus. scan()
 * sweeps addresses 0x08..0x77 under the bus lock and returns those that ACKed, annotated with the
 * driver mounted there (none, until a driver is constructed for the address).
 *
 * Wiring: connect the devices' SDA/SCL to the pins below (3V3 + GND shared). Adjust SDA/SCL to the
 * pins your board exposes for I2C.
 */

use Baremetal\I2c\Bus;

const SDA = 11;
const SCL = 10;

$bus = new Bus(sda: SDA, scl: SCL);

printf("I2C scan (sda=%d scl=%d):\n", SDA, SCL);
$found = $bus->scan();
foreach ($found as $addr => $info) {
    printf("  0x%02X  %s\n", $addr, $info['driver'] ?? '(no driver)');
}
printf("%d device(s)\n", count($found));
