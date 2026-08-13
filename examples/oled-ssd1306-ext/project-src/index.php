<?php
/**
 * SSD1306 128x32 OLED driven by a native PHP extension -- an FPS benchmark.
 *
 * The driver is C: a per-project extension in firmware/exts/ssd1306/ that phpflash compiles into
 * the firmware. The I2C link (hardware I2C peripheral), the framebuffer and the text rendering all
 * live in C; this script just calls the ssd1306_* functions. It is the native counterpart to
 * examples/oled-ssd1306-fps (the same panel, driven by a pure-PHP bit-banged I2C driver) -- run
 * both and compare the frame rate.
 *
 * Wiring (0.91" SSD1306 module, 128x32): VCC -> 3V3, GND -> GND, SDA -> GPIO7, SCL -> GPIO8.
 */

const SDA = 7;      // board pin "SDA / GPIO7"
const SCL = 8;      // board pin "SCL / GPIO8"

$fps = 0.0;         // last measured frames/second, drawn on the panel

function setup(): void
{
    printf("SSD1306 via native C extension (SDA=%d SCL=%d)\n", SDA, SCL);

    if (!function_exists('ssd1306_begin')) {
        echo "ERROR: the ssd1306 extension is not built in -- check firmware/exts/ssd1306/\n";
        return;
    }

    ssd1306_begin(SDA, SCL, 0x3C);
    echo ssd1306_present() ? "panel: present (ACK)\n" : "panel: no ACK -- check wiring\n";

    ssd1306_clear();
    ssd1306_text(4, 4,  'PHP ' . PHP_MAJOR_VERSION . '.' . PHP_MINOR_VERSION);
    ssd1306_text(4, 18, 'C EXT');
    ssd1306_flush();
    delay(800);
    echo "benchmarking full-frame flushes (native driver)...\n";
}

function loop(int $tick): void
{
    global $fps;
    static $sweep = 0;

    // Render and flush as many full frames as we can in a fixed window. Everything in here runs in
    // C now (clear / text / rect / flush); frames / elapsed is the native driver's throughput.
    $count = 0;
    $t0 = microtime(true);
    do {
        ssd1306_clear();
        ssd1306_text(0,  0, 'SSD1306 C EXT');
        ssd1306_text(0,  9, 'PHP ' . PHP_MAJOR_VERSION . '.' . PHP_MINOR_VERSION);
        ssd1306_text(0, 18, 'FPS ' . sprintf('%5.1f', $fps));

        $span = 128 - 14;
        $ph = $sweep % (2 * $span);
        $x = $ph < $span ? $ph : (2 * $span - $ph);
        ssd1306_rect($x, 27, 14, 5);

        ssd1306_flush();
        $sweep++;
        $count++;
        $elapsed = microtime(true) - $t0;
    } while ($elapsed < 0.30);

    $fps = $count / $elapsed;
    printf("draw fps: %6.1f  (%d frames in %d ms)\n", $fps, $count, (int) round($elapsed * 1000));

    delay(10);   // yield one tick between bursts so the task watchdog stays fed
}
