<?php
// A tour proving this is the real PHP -- closures, generators, classes, interfaces,
// match, exceptions and the standard library -- now with each step timed. Copy to
// the microSD as index.php.
//
// Timing note: PHP integers are 32-bit on this RISC-V32 build, so hrtime(true)
// (nanoseconds in one int) would overflow after ~2 seconds. We use hrtime(false),
// which returns [seconds, nanoseconds] with nanoseconds < 1e9, and do the math in
// float (doubles stay 64-bit). Times are shown in milliseconds.

$timer_ok = hrtime(false) !== false;
if (!$timer_ok) {
    echo "note: hrtime() is unavailable on this build; timings are omitted\n";
}
$total_ms = 0.0;

/** Milliseconds between two hrtime(false) readings, computed in float. */
function elapsed_ms(array $t0, array $t1): float {
    return ($t1[0] - $t0[0]) * 1000.0 + ($t1[1] - $t0[1]) / 1_000_000;
}

/** Run $fn (which returns a display string) and print "<label>: <result> (<ms>)". */
function timed(string $label, callable $fn): void {
    global $timer_ok, $total_ms;
    $t0 = hrtime(false);
    $result = $fn();
    if ($timer_ok) {
        $ms = elapsed_ms($t0, hrtime(false));
        $total_ms += $ms;
        $suffix = '  (' . number_format($ms, 3, ',', '') . ' ms)';
    } else {
        $suffix = '';
    }
    printf("%-16s %s%s\n", $label . ':', $result, $suffix);
}

// arrays + arrow functions
$squares = [];
timed('squares', function () use (&$squares) {
    $squares = array_map(fn ($n) => $n * $n, range(1, 10));
    return implode(', ', $squares);
});
timed('sum of squares', fn () => (string) array_sum($squares));

// a generator (not the same thing as a Fiber, so this works)
function fib(): Generator {
    [$a, $b] = [0, 1];
    while (true) {
        yield $a;
        [$a, $b] = [$b, $a + $b];
    }
}
timed('fibonacci', function () {
    $first = [];
    foreach (fib() as $n) {
        if (count($first) >= 10) break;
        $first[] = $n;
    }
    return implode(', ', $first);
});

// classes + interfaces
interface Shape {
    public function area(): float;
}
class Circle implements Shape {
    public function __construct(private float $r) {}
    public function area(): float { return M_PI * $this->r ** 2; }
}
timed('circle area', fn () => (string) round((new Circle(2.0))->area(), 4));

// match
$classify = fn (int $n): string => match (true) {
    $n < 0   => 'negative',
    $n === 0 => 'zero',
    default  => 'positive',
};
timed('classify(-3)', fn () => $classify(-3));

// exceptions
timed('caught', function () {
    try {
        throw new RuntimeException('boom');
    } catch (RuntimeException $e) {
        return $e->getMessage();
    }
});

// standard library
timed('json', fn () => json_encode(['ok' => true, 'squares' => $squares]));
timed('sha1(\'php\')', fn () => sha1('php'));
timed('uppercase', fn () => strtoupper('it really is php'));

if ($timer_ok) {
    printf("%-16s %s ms total\n", 'time:', number_format($total_ms, 3, ',', ''));
}
