# Footprint: where the space goes

How much flash and RAM the engine needs, broken down by area, plus the cost of each optional
extension. The numbers come from real builds for the ESP32-P4. They are rounded and approximate, but
the proportions hold, and they carry over to the ESP32-S3 (the code is the same; only the pool sizes
differ, 8 MB of PSRAM and 16 MB of flash instead of up to 32 MB of each).

## Flash: what fills the roughly 3 MB image

The image runs execute-in-place from flash and is not copied into RAM. These are the approximate
weights of each part of the engine, summed from the compiled objects. They are an upper bound, since
the linker drops unused code, but they show where the mass sits:

| Area | Approx. flash | What it is |
|---|---|---|
| Zend engine | ~1.27 MB | the VM, compiler, GC, object and class system (includes a ~260 KB optimizer) |
| `ext/standard` | ~700 KB | strings, arrays, math, `var_dump`, printf, url, base64, crypt |
| `ext/pcre` | ~320 KB | the PCRE2 regex library |
| `ext/hash` | ~290 KB | the md5/sha/sha3 hash algorithms |
| `main` | ~140 KB | SAPI, streams, INI, output |
| `ext/spl` | ~125 KB | the SPL classes and iterators |
| `ext/reflection` | ~80 KB | the Reflection API |
| `ext/json` | ~25 KB | JSON encode and decode |
| `ext/random` | ~25 KB | the random engines |

The Zend engine, the language itself, is the single biggest piece. Everything else is the standard
library and the always-on extensions.

## The baseline per PHP version

The engine grows from one PHP release to the next, so the starting image does too. These are the same
build (the `hello` init-loop, every optional extension off, ESP32-P4) compiled against each supported
version, changing nothing but `-DPHP_VERSION`:

| Version | Image (`.bin`) | vs 8.3.33 | Static internal RAM |
|---|---|---|---|
| 8.3.33 | ~3.09 MB | baseline | ~109 KB |
| 8.4.24 | ~3.20 MB | +105 KB | ~108 KB |
| 8.5.9 | ~3.29 MB | +198 KB (+93 KB over 8.4) | ~109 KB |

The cost is almost entirely flash (code and read-only data). 8.4 brings the new Zend machinery
(frameless functions, lazy objects, property hooks) on top of 8.3. 8.5 adds more of the same plus
`ext/uri`, which 8.5 makes a core dependency and this port therefore builds into the baseline: the
RFC 3986 parser (uriparser) and the legacy `parse_url()` parser. The WHATWG backend (lexbor) is
dropped, so its ~370 KB of tables are not in these figures; see
[the versions README](../components/php/versions/README.md).

The static internal-RAM footprint does not move with the version (the three are within about 1 KB of
each other), and the PSRAM heap is unaffected. On the P4 the extra flash is immaterial against a 12 MB
app partition; on the S3's 16 MB flash it is still comfortable. The per-extension deltas in the rest of
this document were measured on 8.3.33 and carry over to 8.4 and 8.5 within a few KB.

## Optional extensions

Off by default, enabled per project (phpflash does this from the manifest; by hand it is a
`-DPHP_EXT_*=ON` flag, see [ext-porting.md](ext-porting.md)). The cost is the extra flash each one
adds. All figures are measured image deltas: the `.bin` size with the extension on, minus the baseline
with every optional extension off (around 3.08 MB on 8.3.33; see the per-version baseline above).

| Extension | Adds to flash | Notes |
|---|---|---|
| `ext/date` | ~650 KB | the real `DateTime` and date/time API; about 350 KB of that is the bundled timezone database. Replaces the UTC stub. |
| `ext/date` (UTC-only tz) | ~300 KB | the same with `PHP_EXT_DATE_MINIMAL_TZ`: a UTC-only timezone database, about 350 KB smaller, with no named zones. |
| PDO + SQLite | ~560 KB | about 530 KB of that is the SQLite library, plus ~60 KB `ext/pdo` and ~9 KB `ext/pdo_sqlite`. |
| `ext/ctype` | ~2.5 KB | one source file, no data tables. |
| `ext/filter` | ~27 KB | `filter_var()` validation and sanitization. |
| `ext/tokenizer` | ~13 KB | `token_get_all()` and `PhpToken` over the engine's own lexer. |
| `ext/session` | ~50 KB | `session_start()` and `$_SESSION` with the files and user save handlers. |
| `opcache` | ~500 KB | Zend OPcache (no JIT). File cache on the microSD, or in-RAM in PSRAM. See [opcache.md](opcache.md). |
| `openssl` (subset) | ~42 KB | the mbedTLS-backed subset: symmetric AES (`openssl_encrypt`, `openssl_decrypt`). |
| `openssl` (full) | ~2.1 MB | the real `ext/openssl` on a ported OpenSSL 3.0 libcrypto: RSA, EC, X.509, EVP. Replaces the subset. See [openssl.md](openssl.md). |
| `ext/mbstring` | ~965 KB | the heavy one. Bundles libmbfl, most of it the CJK conversion tables. Built without `mb_ereg` unless oniguruma is enabled. |
| `ext/mbstring` (no CJK) | ~209 KB | the same with `PHP_EXT_MBSTRING_NO_CJK`: drops the legacy CJK codecs (Shift-JIS, EUC, Big5, GB18030, `mb_convert_kana`), about 755 KB smaller. UTF-8, UTF-16 and Latin are unaffected. |
| `ext/mbstring` + `mb_ereg` | +~445 KB | on top of mbstring, with `PHP_EXT_MBSTRING_ONIG`: the `mb_ereg` and `mb_split` multibyte-regex family, which bundles oniguruma. Around 1.38 MB together with full mbstring. |

`ctype`, `filter` and `mbstring` together add about 995 KB; with `date` and PDO/SQLite also on,
everything lands around 5.3 MB (the deltas are measured one at a time, so summing them is
approximate), still well inside a 12 MB app partition. `mbstring` dominates that cost, and dropping
its CJK codecs shrinks it from 965 KB to 209 KB, which brings the three down to about 240 KB together.
Runtime memory stays negligible either way: all of these allocate from PSRAM.

## Networking and the web-server model

These are not extensions; they come with the board and the project type. Networking is linked in for a
board that has it; the `web-server` project type adds an HTTP server on top. Deltas are measured the
same way, against the baseline with all optional extensions off:

| Feature | Adds to flash | Notes |
|---|---|---|
| Ethernet networking | ~103 KB | `esp_eth`, `esp_netif`, `esp_event`, lwIP, and the board's `board_network_up()`. Present on any networked board (the P4-ETH's RMII PHY or the S3-ETH's W5500 over SPI), used by anything that touches the network. |
| `web-server` project type | +~37 KB | on top of networking: the `esp_http_server` front end and the per-request PHP model (`-DPHP_PROJECT_WEB_SERVER=ON`). |
| TLS client (HTTPS) | ~180 KB | the `openssl` `tls` setting: `esp-tls` plus the mbedTLS TLS record layer and the stream-transport factory, on top of the full openssl build. Needs networking. Lets PHP open `https://` streams. The shipped CA bundle (~220 KB) lives with the project source, not in the app image. |

So a networked init-loop firmware where PHP owns the socket is about 103 KB over the baseline, and the
`web-server` project type with an HTTP server in front is about 139 KB over it. Both are small next to
a single extension like `date` or `mbstring`, and far inside the app partition.

The image can also go the other way and get smaller. microSD support (the SD drivers plus the board's
mount code) is on by default and costs about 51 KB; an `embedded` project that runs purely from
internal flash can drop it (`[storage] microsd = false`, the default for embedded). `fatfs` and `vfs`
stay either way, since the embedded image is FAT too. Dropping the SD stack also lets a board with no
card slot build cleanly.

## RAM

Two very different pools.

**Internal SRAM (fast, a few hundred KB).** The engine's static footprint here is small: around 95 KB
of zero-initialized data (`.bss`), about 15 KB of initialized data, and about 72 KB of code that must
live in RAM (`.iram`), roughly 180 KB in total. After startup, most of the internal RAM stays free,
reserved for DMA (the SD card, the SPI Ethernet) and FreeRTOS objects. PHP deliberately does not
allocate here.

**PSRAM (large).** This is the runtime heap: every `malloc`, so all the zvals, HashTables, compiled
opcodes and objects. On the P4 it is up to 32 MB, on the S3 it is 8 MB. That size is what decides how
much a workload can do at once, and it is the one hard ceiling on the S3: a plain application or a live
web server leaves plenty free, but a framework's container-compile step wants more than 8 MB and runs
out.

## Concrete examples

Free heap reported over the serial log after each run:

- **`hello`** on the P4, a linear script: about 32.4 MB of heap free when it finishes. The engine plus
  a small script barely dents the pool.
- **`led-blink`** on the P4, the setup/loop model with the engine resident: about 31.2 MB free, steady
  tick after tick. The running engine and a request hold on the order of 1 MB, and it does not grow.
- **`hello`** on the S3: about 8.5 MB free out of the 8 MB PSRAM pool once loaded, which is the whole
  point of the smaller board. Plain scripts and a web server fit; a full framework does not.

For the P4 a build decision is almost always about flash (does the extension fit, and it does), not
about RAM. On the S3 flash is still roomy, but RAM is the number to keep an eye on.

## How these were measured

- Image sizes: the built `php-esp32.bin` with the extension off versus on.
- Per-area weights: summing `size` (text, rodata, data) over each area's object files. These are
  pre-link, so they slightly overcount against the final image, since `--gc-sections` removes unused
  code.
- Runtime heap: the `heap free` lines in the examples' serial logs.
