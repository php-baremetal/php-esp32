---
eyebrow: 'Docs · Reference'
lede: 'Where the flash and RAM go: per-version baseline image sizes, the cost of each engine area and optional extension, PSRAM usage, and the dynamically generated partition table.'
see_also:
  - href: './porting-notes.md'
    meta: 'Reference'
    label: 'Porting notes'
  - href: '../extensions/porting-status.md'
    meta: 'Extensions'
    label: 'Extension porting status'
  - href: '../extensions/opcache.md'
    meta: 'Extensions'
    label: 'OPcache'
  - href: '../extensions/openssl.md'
    meta: 'Extensions'
    label: 'OpenSSL'
prev:
  label: 'Persist and share state'
  href: '../recipes/persist-state.md'
next:
  label: 'Porting notes'
  href: './porting-notes.md'
---

# Footprint

How much flash and RAM the engine needs, broken down by area, plus the cost of each optional extension and the shape of the generated partition table. The numbers come from real builds for the ESP32-P4. They are rounded and approximate, but the proportions hold, and they carry over to the ESP32-S3: the code is the same, only the pool sizes differ (8 MB of PSRAM and 16 MB of flash instead of up to 32 MB of each).

<!-- @callout variant="info" title="How to read these numbers" -->
Every figure is an approximate, rounded measurement from a real build, not a spec. Per-area weights are pre-link sums and slightly overcount, since `--gc-sections` drops unused code from the final image. Per-extension and per-feature costs are measured image deltas: the `.bin` size with the feature on minus the baseline with every optional extension off. Deltas are measured one at a time, so summing several of them is approximate but close.
<!-- @endcallout -->

## Flash: what fills the roughly 3 MB image

The image runs execute-in-place from flash and is not copied into RAM. These are the approximate weights of each part of the engine, summed from the compiled objects. They are an upper bound, since the linker drops unused code, but they show where the mass sits.

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

The Zend engine, the language itself, is the single biggest piece. Everything else is the standard library and the always-on extensions.

## The baseline per PHP version

The engine grows from one PHP release to the next, so the starting image does too. These are the same build (the `hello` init-loop, every optional extension off, ESP32-P4) compiled against each supported version, changing nothing but `-DPHP_VERSION`.

| Version | Image (`.bin`) | vs 8.3.33 | Static internal RAM |
|---|---|---|---|
| 8.3.33 | ~3.09 MB | baseline | ~109 KB |
| 8.4.24 | ~3.20 MB | +105 KB | ~108 KB |
| 8.5.9 | ~3.29 MB | +198 KB (+93 KB over 8.4) | ~109 KB |

The cost is almost entirely flash (code and read-only data). 8.4 brings the new Zend machinery (frameless functions, lazy objects, property hooks) on top of 8.3. 8.5 adds more of the same plus `ext/uri`, which 8.5 makes a core dependency and this port therefore builds into the baseline: the RFC 3986 parser (uriparser) and the legacy `parse_url()` parser. The WHATWG backend (lexbor) is dropped, so its ~370 KB of tables are not in these figures.

The static internal-RAM footprint does not move with the version (the three are within about 1 KB of each other), and the PSRAM heap is unaffected. On the P4 the extra flash is immaterial against a 12 MB app partition; on the S3's 16 MB flash it is still comfortable. The per-extension deltas in the rest of this document were measured on 8.3.33 and carry over to 8.4 and 8.5 within a few KB.

<!-- @callout variant="note" title="Version selection" -->
The supported versions coexist in the tree; a build picks one with `-DPHP_VERSION`. See the [versions README](../../components/php/versions/README.md) for the dropped-lexbor detail and the full list of what each release adds.
<!-- @endcallout -->

## Optional extensions

Off by default, enabled per project (phpflash does this from the manifest; by hand it is a `-DPHP_EXT_*=ON` flag, see [extension porting status](../extensions/porting-status.md)). The cost is the extra flash each one adds. All figures are measured image deltas: the `.bin` size with the extension on, minus the baseline with every optional extension off (around 3.08 MB on 8.3.33; see the per-version baseline above).

| Extension | Adds to flash | Notes |
|---|---|---|
| `ext/date` | ~650 KB | the real `DateTime` and date/time API; about 350 KB of that is the bundled timezone database. Replaces the UTC stub. |
| `ext/date` (UTC-only tz) | ~300 KB | the same with `PHP_EXT_DATE_MINIMAL_TZ`: a UTC-only timezone database, about 350 KB smaller, with no named zones. |
| PDO + SQLite | ~560 KB | about 530 KB of that is the SQLite library, plus ~60 KB `ext/pdo` and ~9 KB `ext/pdo_sqlite`. |
| `ext/ctype` | ~2.5 KB | one source file, no data tables. |
| `ext/filter` | ~27 KB | `filter_var()` validation and sanitization. |
| `ext/tokenizer` | ~13 KB | `token_get_all()` and `PhpToken` over the engine's own lexer. |
| `ext/session` | ~50 KB | `session_start()` and `$_SESSION` with the files and user save handlers. |
| `opcache` | ~500 KB | Zend OPcache (no JIT). File cache on the microSD, or in-RAM in PSRAM. See [OPcache](../extensions/opcache.md). |
| `openssl` (subset) | ~42 KB | the mbedTLS-backed subset: symmetric AES (`openssl_encrypt`, `openssl_decrypt`). |
| `openssl` (full) | ~2.1 MB | the real `ext/openssl` on a ported OpenSSL 3.0 libcrypto: RSA, EC, X.509, EVP. Replaces the subset. See [OpenSSL](../extensions/openssl.md). |
| `ext/mbstring` | ~965 KB | the heavy one. Bundles libmbfl, most of it the CJK conversion tables. Built without `mb_ereg` unless oniguruma is enabled. |
| `ext/mbstring` (no CJK) | ~209 KB | the same with `PHP_EXT_MBSTRING_NO_CJK`: drops the legacy CJK codecs (Shift-JIS, EUC, Big5, GB18030, `mb_convert_kana`), about 755 KB smaller. UTF-8, UTF-16 and Latin are unaffected. |
| `ext/mbstring` + `mb_ereg` | +~445 KB | on top of mbstring, with `PHP_EXT_MBSTRING_ONIG`: the `mb_ereg` and `mb_split` multibyte-regex family, which bundles oniguruma. Around 1.38 MB together with full mbstring. |

`ctype`, `filter` and `mbstring` together add about 995 KB; with `date` and PDO/SQLite also on, everything lands around 5.3 MB (the deltas are measured one at a time, so summing them is approximate), still well inside a 12 MB app partition. `mbstring` dominates that cost, and dropping its CJK codecs shrinks it from 965 KB to 209 KB, which brings the three down to about 240 KB together. Runtime memory stays negligible either way: all of these allocate from PSRAM.

<!-- @callout variant="tip" title="Shrinking a build" -->
Three levers move the image size down. Drop extensions you do not use (each is off by default; `mbstring` and full `openssl` are the two big ones). Prefer the trimmed variants where they exist — `PHP_EXT_MBSTRING_NO_CJK` (-755 KB), `PHP_EXT_DATE_MINIMAL_TZ` (-350 KB), or the `openssl` subset instead of the full build (~42 KB vs ~2.1 MB). Build `embedded` rather than networked when the board does not need the network. And an `embedded` project that runs purely from internal flash can drop the SD stack with `-DPHP_STORAGE_MICROSD=OFF` (`[storage] microsd = false`), saving about 51 KB.
<!-- @endcallout -->

## Networking and the web-server model

These are not extensions; they come with the board and the project type. Networking is linked in for a board that has it; the `web-server` project type adds an HTTP server on top. Deltas are measured the same way, against the baseline with all optional extensions off.

| Feature | Adds to flash | Notes |
|---|---|---|
| Ethernet networking | ~103 KB | `esp_eth`, `esp_netif`, `esp_event`, lwIP, and the board's `board_network_up()`. Present on any networked board (the P4-ETH's RMII PHY or the S3-ETH's W5500 over SPI), used by anything that touches the network. |
| `web-server` project type | +~37 KB | on top of networking: the `esp_http_server` front end and the per-request PHP model (`-DPHP_PROJECT_WEB_SERVER=ON`). |
| TLS client (HTTPS) | ~180 KB | the `openssl` `tls` setting: `esp-tls` plus the mbedTLS TLS record layer and the stream-transport factory, on top of the full openssl build. Needs networking. Lets PHP open `https://` streams. The shipped CA bundle (~220 KB) lives with the project source, not in the app image. |

So a networked init-loop firmware where PHP owns the socket is about 103 KB over the baseline, and the `web-server` project type with an HTTP server in front is about 139 KB over it. Both are small next to a single extension like `date` or `mbstring`, and far inside the app partition.

The image can also go the other way and get smaller. microSD support (the SD drivers plus the board's mount code) is on by default and costs about 51 KB; an `embedded` project that runs purely from internal flash can drop it (`[storage] microsd = false`, the default for embedded). `fatfs` and `vfs` stay either way, since the embedded image is FAT too. Dropping the SD stack also lets a board with no card slot build cleanly.

## Partitions

The flash layout is generated per build (`cmake/gen-partitions.cmake`), not fixed. A board's `partitions.csv` only pins the constant partitions; the `storage` partition — the read-only FAT image that holds an embedded project's PHP source — is generated, and the optional `phpstore` NVS partition is added only when a project asks for it. The generated table lands in the project's `build/` (`partitions.gen.csv`) and is never committed.

### The constant partitions

Every board's committed `partitions.csv` pins the same three fixed partitions; only the `factory` app size differs between the P4 (32 MB flash) and the S3 (16 MB flash).

| Partition | Type / subtype | Size | Notes |
|---|---|---|---|
| `nvs` | data / nvs | 24 KB | the system NVS namespace |
| `phy_init` | data / phy | 4 KB | RF calibration data |
| `factory` | app / factory | 12 MB (P4) · 10 MB (S3) | the app image; the ~3 MB engine sits here with plenty of room |

<!-- @code-block language="text" label="A board partitions.csv (esp32-p4-eth) — only the fixed partitions" -->
```text
# Name,     Type, SubType,  Offset,   Size
nvs,        data, nvs,      ,         24K
phy_init,   data, phy,      ,         4K
factory,    app,  factory,  ,         12M
```
<!-- @endcode-block -->

### The generated `storage` partition

The `storage` partition is regenerated (or dropped) per build depending on the project type.

| Project type | `storage` partition | Sizing |
|---|---|---|
| `embedded` | added | sized to the source, not a fixed block: cluster-rounded occupancy + FAT overhead + optional `reserve_kb`, aligned up to 64 KB with a 128 KB floor |
| `microsd` | omitted | the source lives on the card; the firmware looks the partition up by name and falls back to the card when absent, so flash stays free |

For an `embedded` project the size is the source's cluster-rounded occupancy (4 KB FAT clusters, so each file rounds up), plus 64 KB of FAT overhead (boot sector, FAT tables, root directory), plus the optional `[storage] reserve_kb`, aligned up to 64 KB with a 128 KB floor (fatfsgen needs a valid minimum). A tiny sketch gets a 128 KB partition instead of the old fixed multi-megabyte one.

<!-- @code-block language="toml" label="Reserve headroom in the storage image for files written at runtime" -->
```toml
[storage]
reserve_kb = 256   # added on top of the source occupancy before alignment
```
<!-- @endcode-block -->

So a microSD firmware on the P4 leaves roughly 20 MB of the 32 MB flash unpartitioned, and an embedded one only spends what the source needs.

### The optional `phpstore` NVS partition

The reboot-persistent key-value store (the `store` extension) gets its own NVS partition, added only when the project sets `[store] size_kb` (flash-tool passes `-DPHP_STORE_KB`). The requested size is aligned to 4 KB with a 16 KB floor (NVS needs a few pages to be useful). Absent or zero means no partition at all, and `store_available()` returns false. It is independent of the embedded/microSD choice.

| Partition | Type / subtype | Size | Condition |
|---|---|---|---|
| `phpstore` | data / nvs | `size_kb`, 4 KB-aligned, 16 KB floor | present only when `[store] size_kb > 0` |

<!-- @code-block language="toml" label="Request a persistent store partition" -->
```toml
[store]
size_kb = 64
```
<!-- @endcode-block -->

## RAM

Two very different pools.

**Internal SRAM (fast, a few hundred KB).** The engine's static footprint here is small: around 95 KB of zero-initialized data (`.bss`), about 15 KB of initialized data, and about 72 KB of code that must live in RAM (`.iram`), roughly 180 KB in total. After startup, most of the internal RAM stays free, reserved for DMA (the SD card, the SPI Ethernet) and FreeRTOS objects. PHP deliberately does not allocate here.

| Segment | Approx. size | What it is |
|---|---|---|
| `.bss` | ~95 KB | zero-initialized static data |
| `.data` | ~15 KB | initialized static data |
| `.iram` | ~72 KB | code that must execute from RAM |
| Total static | ~180 KB | reserved out of the internal SRAM pool at startup |

**PSRAM (large).** This is the runtime heap: every `malloc`, so all the zvals, HashTables, compiled opcodes and objects. On the P4 it is up to 32 MB, on the S3 it is 8 MB. That size is what decides how much a workload can do at once, and it is the one hard ceiling on the S3: a plain application or a live web server leaves plenty free, but a framework's container-compile step wants more than 8 MB and runs out.

## Concrete examples

Free heap reported over the serial log after each run.

| Example | Board | Heap free after run | Note |
|---|---|---|---|
| `hello` | P4 | ~32.4 MB | a linear script barely dents the pool |
| `led-blink` | P4 | ~31.2 MB | setup/loop model, engine resident; holds ~1 MB, does not grow |
| `hello` | S3 | ~8.5 MB (of 8 MB PSRAM) | plain scripts and a web server fit; a full framework does not |

For the P4 a build decision is almost always about flash (does the extension fit, and it does), not about RAM. On the S3 flash is still roomy, but RAM is the number to keep an eye on.

## How these were measured

- **Image sizes**: the built `php-esp32.bin` with the extension off versus on.
- **Per-area weights**: summing `size` (text, rodata, data) over each area's object files. These are pre-link, so they slightly overcount against the final image, since `--gc-sections` removes unused code.
- **Runtime heap**: the `heap free` lines in the examples' serial logs.
