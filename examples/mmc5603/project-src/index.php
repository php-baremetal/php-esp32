<?php
// MMC5603 3-axis magnetometer: field in uT and compass heading.

use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Mmc5603;

$bus = new Bus(sda: 7, scl: 8);   // MMC5603 on the board

echo "scan:\n";
foreach ($bus->scan() as $addr => $info) {
    printf("  0x%02X  %s\n", $addr, $info['driver'] ?? '(no driver)');
}

$mag = new Mmc5603($bus);

function loop(): void
{
    global $mag;

    [$x, $y, $z] = $mag->mag();
    $b = sqrt($x * $x + $y * $y + $z * $z);
    printf("mag = [% 7.1f % 7.1f % 7.1f] uT  |B| = %5.1f  heading = %3.0f deg\n",
        $x, $y, $z, $b, $mag->heading());
    usleep(500000);
}
