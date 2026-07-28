# language-tour

A quick tour through the parts of PHP that a "toy subset" would never have: closures,
arrow functions, generators, classes and interfaces, `match`, exceptions and a slice of
the standard library. It also times every step, so you can see how fast real PHP runs on
the chip.

## What it does

Each feature is exercised inside a small `timed()` helper that runs it and prints the
result together with how long it took, in milliseconds. At the end it prints the total.

```php
function elapsed_ms(array $t0, array $t1): float {
    return ($t1[0] - $t0[0]) * 1000.0 + ($t1[1] - $t0[1]) / 1_000_000;
}

function timed(string $label, callable $fn): void {
    global $total_ms;
    $t0 = hrtime(false);
    $result = $fn();
    $ms = elapsed_ms($t0, hrtime(false));
    $total_ms += $ms;
    printf("%-16s %s  (%s ms)\n", $label . ':', $result, number_format($ms, 3, ',', ''));
}
```

The tour itself covers array/arrow-function work, a Fibonacci generator, a `Circle`
implementing a `Shape` interface, a `match` expression, a caught exception, and
`json_encode` / `sha1` / `strtoupper` from the standard library.

## What it demonstrates

- **It really is full PHP, on a microcontroller.** Generators, interfaces, `match`,
  exceptions and the standard library all work; this is the stock 8.3 engine, not a
  cut-down clone.
- **You can measure it, and it's fast.** Each step runs in well under a millisecond and
  the whole tour finishes in a few milliseconds on the RISC-V core.

### About the timing

PHP integers are **32-bit** on this build (RISC-V32 is ILP32), so `hrtime(true)` — which
returns nanoseconds packed into a single integer — would overflow after about two seconds.
The example uses `hrtime(false)` instead, which returns `[seconds, nanoseconds]` with the
nanoseconds always below `1e9` (comfortably inside a 32-bit int), and does the subtraction
in floating point (doubles stay 64-bit). Times are printed in milliseconds with a comma as
the decimal separator.

## The output

From [`monitor.txt`](monitor.txt) (the full serial log):

```
PHP 8.3.32 on ESP32-P4
--- /sdcard/index.php ---
squares:         1, 4, 9, 16, 25, 36, 49, 64, 81, 100  (0,682 ms)
sum of squares:  385  (0,094 ms)
fibonacci:       0, 1, 1, 2, 3, 5, 8, 13, 21, 34  (0,693 ms)
circle area:     12.5664  (0,539 ms)
classify(-3):    negative  (0,061 ms)
caught:          boom  (0,662 ms)
json:            {"ok":true,"squares":[1,4,9,16,25,36,49,64,81,100]}  (0,334 ms)
sha1('php'):     47425e4490d1548713efea3b8a6f5d778e4b1766  (0,396 ms)
uppercase:       IT REALLY IS PHP  (0,061 ms)
time:            3,522 ms total
--- end ---
php-esp32: done -- heap free: 33980391 bytes
```

## Running it

Copy `index.php` to the microSD (in the root, as `index.php`), put the card back in the
board, and press reset. No wiring needed.
