# benchmark

On-device measurements for the questions people ask about running PHP on the chip:
how much a **compiled script** costs in memory, how much memory a **workload** uses at
runtime, and how fast a **tight PHP loop** can toggle a GPIO. It prints one report at boot
and then idles.

It answers, with real numbers, the kind of questions raised in
[discussion #3](https://github.com/orgs/php-baremetal/discussions/3).

## What it measures

Memory is read with the `baremetal_utility` extension (`psram_free()`, `heap_free()`, ...). On this
port the Zend heap lives in PSRAM via malloc (`USE_ZEND_ALLOC=0`), so PHP's own `memory_get_usage()`
reads 0 — watching PSRAM free go down is the real figure, and it counts everything, not just PHP's arena.

1. **Engine baseline** — `psram_free()` right after boot: how much PSRAM the running engine already
   holds before your code does anything.
2. **Compiled footprint** — for three reference files (`bench_small/medium/large.php`) it prints the
   source size and the PSRAM that `require`-ing them consumes (definitions only). That delta is the
   compiled form: op_arrays, the class entry, interned strings. Interned strings are shared, so files
   after the first can read a little low.
3. **Execution memory** — a representative workload (5000 rows, sorted) and the PSRAM working set it
   holds while alive.
4. **GPIO toggle rate** — a tight `for` loop of `gpio_write()` calls, timed with `hrtime()`. It then
   runs the *same* loop in a C project-extension (`firmware/exts/native_gpio/`) and prints both, so
   the gap — the interpreter's per-call cost — is right there in the output.

## Reading the numbers

- **PSRAM size/speed and CPU frequency** are printed by the firmware in the boot log, just above
  this report — note them down, because every number here depends on them.
- The GPIO rate is **interpreter-bound** (no JIT) and lands in the **kHz** range, not MHz. It moves
  with CPU frequency, with whether opcache is warm, and with PSRAM latency (opcodes and variables
  live in PSRAM). For hard, fast or precisely-timed signals, drive a peripheral (RMT/LEDC/I²S) or
  add a small C project-extension instead of bit-banging from PHP.
- Runtime memory here is the S3's 8 MB PSRAM. A full framework's container-compile step wants more
  than that; those figures need a 32 MB ESP32-P4.

## Measured (reference numbers)

Same firmware (PHP 8.4.25, no opcache), on three boards that span the PSRAM range. The compiled
footprint and the per-row cost are **engine-only** — an op_array is the same size on Xtensa and
RISC-V, so they're identical everywhere. What changes is the GPIO rate (it tracks CPU frequency) and
how much room is left for your data (total PSRAM).

| | ESP32-S3  minimal board ("supermini") | ESP32-S3 | ESP32-P4 |
|---|---|---|---|
| PSRAM | 2 MB Quad @ 80 MHz | 8 MB Octal @ 80 MHz | 32 MB @ 200 MHz |
| CPU | 160 MHz (Xtensa) | 160 MHz (Xtensa) | 360 MHz (RISC-V) |
| engine baseline (PSRAM used) | ~1.3 MB | ~1.3 MB | ~1.3 MB |
| free for your data | ~0.7 MB | ~6.9 MB | ~31.5 MB |
| compile `bench_small` (17 ln / 0.4 KB) | 2.3 KB | 2.3 KB | 2.3 KB |
| compile `bench_medium` (76 ln / 2.0 KB) | 10.3 KB | 10.3 KB | 10.3 KB |
| compile `bench_large` (167 ln / 4.2 KB) | 22.4 KB | 22.4 KB | 22.4 KB |
| GPIO — PHP loop | **~155 kHz** (3229 ns/write) | **~155 kHz** (3229 ns) | **~279 kHz** (1810 ns) |
| GPIO — C ext (`native_gpio`) | **~1.4 MHz** (357 ns) | **~1.4 MHz** (357 ns) | **~1.35 MHz** (371 ns) |
| C-over-PHP speedup | 9× | 9× | 5× |
| 5000-row working set (~1.83 MB) | doesn't fit → ~1091 rows / 0.4 MB | 1.83 MB | 1.83 MB |

Three things stand out:

- **A compiled script costs ~5× its source** in PSRAM (op_arrays + class entry + interned strings) —
  the same on every board, since it's the engine, not the chip.
- **Two different ceilings — and they don't scale the same way.** The PHP loop is CPU-bound: it
  tracks the clock (~155 kHz on the 160 MHz S3 boards, ~279 kHz on the 360 MHz P4). The *same* loop in
  the C extension (`firmware/exts/native_gpio/`) sits at **~1.4 MHz on all three** — it's bounded by
  `gpio_set_level()`'s peripheral access (~360 ns), not the core, so a faster chip barely moves it.
  That's why the C-over-PHP gap is **9× on the S3 but only 5× on the P4**: the PHP side sped up with
  the clock, the C side didn't. Both loops call the same `gpio_set_level()`, so the whole gap is the
  interpreter tax (opcode dispatch + a userland call per iteration). Raw register writes
  (`GPIO.out_w1ts`/`w1tc`) go higher still, into the tens of MHz. So for MHz-class or precisely-timed
  signals, a small C extension or a peripheral (RMT/LEDC/I²S) is the win.
- **The ceiling is total PSRAM.** All three spend ~1.3 MB on the engine, so the Super Mini's 2 MB
  leaves only ~0.7 MB for your data (the 5000-row set doesn't fit — the benchmark adapts down); the
  8 MB S3 runs plain apps and a web server; a framework's container-compile step needs the P4's 32 MB.

## Options (top of `index.php`)

- `BENCH_PIN` — the GPIO the toggle test drives (any free pin; put a scope/logic analyzer here to
  confirm the frequency against the printed number).
- `GPIO_ITERS` — how many writes to time.
- `SCOPE_SECONDS` — set > 0 to emit a continuous square wave on `BENCH_PIN` for that many seconds
  after the measurement, so an external analyzer can capture it.

## Run it

Pick your board and flash it with [`phpflash`](https://github.com/php-baremetal/flash-tool):

```sh
phpflash build
phpflash flash
phpflash monitor
```

The default config targets an ESP32-P4; change `[board] target` (and `storage_type` to `embedded`
if you have no microSD) for your board.
