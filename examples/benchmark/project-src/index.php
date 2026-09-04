<?php
// On-device benchmark: real measurements for the questions people ask about
// running PHP on the chip -- how much a compiled script costs, how much memory
// a workload uses, and how fast a tight PHP loop can toggle a GPIO.
//
// Memory is read with the baremetal_utility extension (bm_psram_free(), etc.): on
// this port the Zend heap lives in PSRAM via malloc (USE_ZEND_ALLOC=0), so PHP's
// own memory_get_usage() reads 0. Watching PSRAM free go down is the real figure.
//
// init-loop model: setup() runs the measurements once; loop() then idles. The
// memory-heavy workload runs last and adapts to the PSRAM on hand, so the report
// still comes through on a 2 MB board (S3-Zero) as well as a 32 MB one (P4).

const BENCH_PIN     = 2;      // any free GPIO; put a scope here for the square wave
const GPIO_ITERS    = 100000; // writes in the tight-loop timing
const SCOPE_SECONDS = 0;      // >0: after measuring, emit a tight square wave this long

function kb(int $b): string { return number_format($b / 1024, 1) . ' KB'; }

// Bytes of PSRAM consumed by compiling one source file (definitions only ->
// op_arrays + class entry + interned strings).
function measure_compile(string $file): array {
    $bytes = filesize($file);
    $lines = count(file($file));
    $before = bm_psram_free();
    require $file;
    return [$lines, $bytes, $before - bm_psram_free()];
}

// Build a data set of $n sorted rows and return it (kept alive so we can read
// the working-set size before it's freed).
function build_workload(int $n): array {
    $a = [];
    for ($i = 0; $i < $n; $i++) {
        $a[] = ['id' => $i, 'name' => "item_$i", 'v' => ($i * 7) % 1000];
    }
    usort($a, fn($x, $y) => $x['v'] <=> $y['v']);
    return $a;
}

function setup(): void {
    $dir = __DIR__;
    echo "==== php-baremetal benchmark ====\n";
    echo 'PHP ' . PHP_VERSION . "\n";
    echo "(PSRAM size/speed and CPU freq are in the boot log above)\n\n";

    // 1) engine baseline
    printf("PSRAM: %s free of %s   |   internal RAM: %s free of %s\n\n",
        kb(bm_psram_free()), kb(bm_psram_size()), kb(bm_heap_free()), kb(bm_heap_size()));

    // 2) compiled footprint, per script
    echo "compiled footprint (PSRAM consumed to compile a source file):\n";
    echo "  file              lines    source   compiled\n";
    foreach (['bench_small.php', 'bench_medium.php', 'bench_large.php'] as $f) {
        [$lines, $bytes, $delta] = measure_compile("$dir/$f");
        printf("  %-16s  %5d   %7s   %8s\n", $f, $lines, kb($bytes), kb($delta));
    }
    echo "\n";

    // 3) GPIO toggle rate from a tight PHP loop (cheap on memory -> runs before the workload)
    gpio_mode(BENCH_PIN, GPIO_OUTPUT);
    $t0 = hrtime(true);
    for ($i = 0; $i < GPIO_ITERS; $i++) {}                       // empty-loop baseline
    $empty = hrtime(true) - $t0;
    $t0 = hrtime(true);
    for ($i = 0; $i < GPIO_ITERS; $i++) { gpio_write(BENCH_PIN, $i & 1); }
    $io = hrtime(true) - $t0;

    $wps = GPIO_ITERS / ($io / 1e9);
    $php_ns = $io / GPIO_ITERS;
    echo 'GPIO tight loop (' . GPIO_ITERS . ' writes on GPIO ' . BENCH_PIN . "):\n";
    printf("  PHP loop:     %.0f ns/write  ->  %s writes/s  ~%s Hz  (empty loop %.0f ns)\n",
        $php_ns, number_format($wps), number_format($wps / 2), $empty / GPIO_ITERS);

    // the same toggle loop, but in a C project-extension (firmware/exts/native_gpio) -- the gap is
    // the interpreter's per-call cost. Guarded so the report still runs if the ext isn't compiled in.
    if (function_exists('native_gpio_toggle_ns')) {
        $c_ns = native_gpio_toggle_ns(BENCH_PIN, 2000000);
        $c_wps = 1e9 / $c_ns;
        printf("  native C:     %.0f ns/write  ->  %s writes/s  ~%s Hz  (%.0fx faster than PHP)\n",
            $c_ns, number_format($c_wps), number_format($c_wps / 2), $php_ns / $c_ns);
    }
    echo "\n";

    if (SCOPE_SECONDS > 0) {
        echo 'emitting a square wave on GPIO ' . BENCH_PIN . ' for ' . SCOPE_SECONDS . "s...\n";
        $end = hrtime(true) + SCOPE_SECONDS * 1000000000;
        $i = 0;
        while (hrtime(true) < $end) { gpio_write(BENCH_PIN, ++$i & 1); }
    }

    // 4) execution working set -- last, and sized to the PSRAM on hand (~400 B/row) so a
    //    2 MB board doesn't run out mid-report.
    $free = bm_psram_free();
    $rows = (int) min(5000, max(500, $free * 0.6 / 400));
    $before = bm_psram_free();
    $data = build_workload($rows);
    $peak = $before - bm_psram_free();
    unset($data);
    printf("execution: %s-row working set = %s in PSRAM (%s was free)\n",
        number_format($rows), kb($peak), kb($free));

    echo "\n==== done ====\n";
}

function loop(int $tick): void {
    delay(1000); // measurements are done; idle.
}
