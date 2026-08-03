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

All figures are measured image deltas: the `.bin` size with the extension on, minus the
baseline (all optional extensions off, ~3.08 MB).

| Extension | Adds to flash | Notes |
|---|---|---|
| `ext/date` | **~650 KB** | the real `DateTime` and date/time API; ~350 KB of that is the builtin timezone database. Replaces the UTC stub. |
| `ext/date` (UTC-only tz) | **~300 KB** | the same, with `PHP_EXT_DATE_MINIMAL_TZ`: a UTC-only timezone database instead of the full one, ~350 KB smaller. No named zones. |
| PDO + SQLite | **~560 KB** | of that, ~530 KB is the SQLite library itself, ~60 KB `ext/pdo`, ~9 KB `ext/pdo_sqlite`. |
| `ext/ctype` | **~2.5 KB** | tiny: one source file, no data tables. |
| `ext/filter` | **~27 KB** | `filter_var()` validation/sanitization. |
| `ext/mbstring` | **~965 KB** | the heavy one. Bundled libmbfl, most of it the CJK conversion tables. Built without `mb_ereg*` (no oniguruma). |
| `ext/mbstring` (no CJK) | **~209 KB** | the same, with `PHP_EXT_MBSTRING_NO_CJK`: drops the legacy CJK codecs (Shift-JIS, EUC-*, Big5, GB18030, `mb_convert_kana`), ~755 KB smaller. UTF-8/UTF-16/Latin unaffected. |
| `ext/mbstring` + `mb_ereg*` | **+~445 KB** | on top of mbstring, with `PHP_EXT_MBSTRING_ONIG`: the `mb_ereg*`/`mb_split` multibyte-regex family, which bundles the oniguruma library. About ~1.38 MB together with full mbstring. |

The last three (`ctype` + `filter` + `mbstring`) add ~995 KB together; with `date` and
`pdo`/`sqlite` also on, everything together lands around ~5.3 MB (the deltas are measured one
at a time, so summing them is approximate) — still well inside the 12 MB app partition.
`mbstring` dominates that cost; dropping its CJK codecs (`PHP_EXT_MBSTRING_NO_CJK`) shrinks it
from ~965 KB to ~209 KB, bringing the three down to ~240 KB together. Runtime memory stays
negligible: all of these allocate from the 32 MB PSRAM pool.

## Networking and the web-server model

Not extensions — these come with the board and the project type. Networking is linked in for a
board that has it (the `esp32-p4-eth`); the `web-server` project type adds an HTTP server on top.
Deltas are measured the same way (image size minus the ~3.08 MB baseline, all optional extensions
off):

| Feature | Adds to flash | Notes |
|---|---|---|
| Ethernet networking | **~103 KB** | `esp_eth` + `esp_netif` + `esp_event` + lwIP + the board's `board_network_up()` (IP101 PHY, DHCP). Present on the `esp32-p4-eth` board; used by anything that touches the network — including the `web-server-init-loop` example, where PHP itself runs the socket server. |
| `web-server` project type | **+~37 KB** | on top of networking: the `esp_http_server` front end and the per-request PHP model (`-DPHP_PROJECT_WEB_SERVER=ON`). |

So a networked init-loop firmware (PHP owns the socket) is about **~103 KB** over the baseline, and
the `web-server` project type (HTTP server in front) is about **~139 KB** over it — measured on the
`esp32-p4-eth` with no optional PHP extensions, `php-esp32.bin` going 3.08 MB → 3.19 MB → 3.23 MB.
Small next to a single extension like `date` or `mbstring`, and far inside the 12 MB app partition.

The other way, you can make the image **smaller**: microSD support (the SDMMC drivers + the board's
mount code) is on by default but costs about **~51 KB**, and an `embedded` project that runs purely
from internal flash can drop it (`-DPHP_STORAGE_MICROSD=OFF`, or `[storage] microsd = false` — the
default for embedded). `fatfs`/`vfs` stay either way, since the embedded image is FAT too. It also
lets a board with no card slot build without pulling the SD stack.

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
