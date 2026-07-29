# Footprint: where the space goes

How much flash and RAM the engine needs, broken down by area, plus the cost of the optional
extensions. Numbers are from a real build for the ESP32-P4 (RISC-V32); they're rounded and
approximate, but the proportions are accurate.

## Flash: what fills the ~3 MB image

The image runs execute-in-place from flash (it isn't copied into RAM). These are the
approximate weights of each part of the engine, measured from the compiled objects. The
figures are an upper bound — the linker drops unused code — but they show where the mass is:

| Area | Approx. flash | What it is |
|---|---|---|
| Zend engine | ~1.27 MB | the VM, compiler, GC, object/class system (incl. ~260 KB optimizer) |
| `ext/standard` | ~700 KB | strings, arrays, math, `var_dump`, printf, url, base64, crypt... |
| `ext/pcre` | ~320 KB | the PCRE2 regex library |
| `ext/hash` | ~290 KB | md5/sha/sha3/... hash algorithms |
| `main` | ~140 KB | SAPI, streams, INI, output |
| `ext/spl` | ~125 KB | the SPL classes and iterators |
| `ext/reflection` | ~80 KB | the Reflection API |
| `ext/json` | ~25 KB | JSON encode/decode |
| `ext/random` | ~25 KB | the random engines |

The Zend engine — the actual language — is the single biggest piece. Everything else is the
standard library and the built-in extensions.

## Optional extensions

Off by default; enabled at build time (see [flash.md](flash.md) and the README). Cost is the
extra flash they add to the image:

| Extension | Adds to flash | Notes |
|---|---|---|
| PDO + SQLite | **~560 KB** | measured image delta. Of that, ~530 KB is the SQLite library itself, ~60 KB `ext/pdo`, ~9 KB `ext/pdo_sqlite`. |

Adding it takes the image from ~3.1 MB to ~3.7 MB — still a small fraction of the 12 MB
partition. Runtime memory is negligible: SQLite is built for small devices, and its heap
comes from the 32 MB PSRAM pool.

## RAM

Two very different pools.

**Internal SRAM (768 KB, fast).** The engine's static footprint is small: about 95 KB of
zero-initialized data (`.bss`), ~15 KB of initialized data, and ~72 KB of code that has to
live in RAM (`.iram`) — roughly **180 KB** in total. After startup around 450 KB of internal
RAM stays free, kept for DMA (the SD card) and FreeRTOS objects. PHP deliberately does **not**
allocate here.

**PSRAM (32 MB, large).** This is where PHP's runtime heap lives — every `malloc`, so all the
zvals, HashTables, compiled opcodes and objects. It's huge relative to what a script needs.

## A couple of concrete examples

Real numbers from the example runs (free heap reported over the serial log):

- **`hello`** (a linear script): after it finishes, ~32.4 MB of heap is free. The whole engine
  plus a small script barely dents the pool.
- **`led-blink`** (the `setup()`/`loop()` model, engine resident and looping): ~31.2 MB free,
  steady tick after tick — the running engine and request hold on the order of ~1 MB, and it
  doesn't grow.

So a build decision is almost always about flash (does the extension fit — yes, easily), not
about RAM (there's more than a script will ever use).

## How these were measured

- Image sizes: the built `php-esp32.bin` with the extension off vs on.
- Per-area weights: summing `size` (text + rodata + data) over each area's object files. These
  are pre-link, so they slightly overcount vs. the final image (the linker's `--gc-sections`
  removes unused code).
- Runtime heap: the `heap free` lines in the examples' `monitor.txt`.
