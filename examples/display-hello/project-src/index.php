<?php
/**
 * Minimal SPI/QSPI panel proof-of-concept: bring up an ST77916 and cycle the background colour.
 *
 * Baremetal\Spi\Bus owns the SPI host + shared pins (QSPI: four data lines). The ST77916 driver is a
 * device on that bus implementing Output\Display. This only proves the panel lights up and paints;
 * text/graphics come later. Set the pins and the panel size to your hardware.
 */

use Baremetal\Spi\Bus;
use Baremetal\Spi\Driver\St77916;
use Baremetal\Output\Display;

$bus = new Bus(sclk: 40, mosi: 46, miso: 45, data2: 42, data3: 41);   // QSPI: mosi=D0, miso=D1
$lcd = new St77916($bus, cs: 21, rst: 3, bl: 5, width: 360, height: 360);

printf("St77916 instanceof Output\\Display: %s\n", $lcd instanceof Display ? "yes" : "no");

function loop(int $tick): void
{
    global $lcd;
    $colors = [$lcd->rgb(255, 0, 0), $lcd->rgb(0, 255, 0), $lcd->rgb(0, 0, 255), $lcd->rgb(255, 255, 255)];
    $lcd->fill($colors[$tick % 4]);
    delay(1000);
}
