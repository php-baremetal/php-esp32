<?php
/**
 * SSD1306 128x32 OLED driven straight from PHP -- an FPS benchmark.
 *
 * The driver lives in SSD1306.php: the I2C link is bit-banged in PHP over two GPIO pins with the
 * `gpio` extension, no C-side driver. This file is just the demo -- it wires the panel up and
 * measures how many full frames a second the pure-PHP driver can push. A later example will drive
 * the same panel through a native extension for comparison.
 *
 * Wiring (0.91" SSD1306 module, 128x32): VCC -> 3V3, GND -> GND, SDA -> GPIO7, SCL -> GPIO8.
 */

require __DIR__ . '/SSD1306.php';

const SDA = 7;      // board pin "SDA / GPIO7"
const SCL = 8;      // board pin "SCL / GPIO8"

$oled = null;       // the SSD1306 driver instance
$fps  = 0.0;        // last measured frames/second, drawn on the panel

function setup(): void
{
    global $oled;
    printf("SSD1306 128x32 over PHP-bit-banged I2C (SDA=%d SCL=%d)\n", SDA, SCL);

    $oled = new SSD1306(SDA, SCL, 0x3C);
    echo $oled->present()
        ? "panel: present (ACK)\n"
        : "panel: no ACK -- check wiring/address; driving blind, FPS still measured\n";

    $oled->begin();
    $oled->clear();
    $oled->text(4, 4,  'PHP ' . PHP_MAJOR_VERSION . '.' . PHP_MINOR_VERSION);
    $oled->text(4, 18, 'OLED FPS');
    $oled->flush();
    delay(800);
    echo "benchmarking full-frame flushes...\n";
}

function loop(int $tick): void
{
    global $oled, $fps;
    static $sweep = 0;

    // Render and flush as many full frames as we can in a fixed window. All the time in here is
    // real drawing (no delay), so frames / elapsed is the honest throughput of the pure-PHP driver.
    $count = 0;
    $t0 = microtime(true);
    do {
        draw_frame($oled, $sweep++, $fps);
        $oled->flush();
        $count++;
        $elapsed = microtime(true) - $t0;
    } while ($elapsed < 0.30);

    $fps = $count / $elapsed;
    printf("draw fps: %5.1f  (%d frames in %d ms)\n", $fps, $count, (int) round($elapsed * 1000));

    delay(10);   // yield one tick between bursts so the task watchdog stays fed
}

/** Draw one animation frame: the labels, the live FPS, and a block that sweeps left-right. */
function draw_frame(SSD1306 $oled, int $n, float $fps): void
{
    $oled->clear();
    $oled->text(0,  0, 'SSD1306 128x32');
    $oled->text(0,  9, 'PHP ' . PHP_MAJOR_VERSION . '.' . PHP_MINOR_VERSION);
    $oled->text(0, 18, 'FPS ' . sprintf('%4.1f', $fps));

    $span = SSD1306::WIDTH - 14;
    $ph = $n % (2 * $span);
    $x = $ph < $span ? $ph : (2 * $span - $ph);
    $oled->rect($x, 27, 14, 5);
}
