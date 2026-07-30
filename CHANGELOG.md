# Changelog

## [0.1.2] 2026-07-30

### Added
- **`ext/date`** as an optional extension (off by default): the real `DateTime` and date/time
  API instead of the UTC stub, ~650 KB of flash. When on, timelib replaces the stub, and one
  config change was needed (`HAVE_STRUCT_TM_TM_GMTOFF`/`_TM_ZONE` off — newlib's `struct tm`
  lacks those fields).
- `PHP_EXT_DATE_MINIMAL_TZ`: a sub-option (asked by `flash.sh` when `date` is on) that ships a
  UTC-only timezone database (~2.7 KB) instead of the full one, saving ~350 KB. Named zones
  (e.g. `Europe/Rome`) then report an error; UTC works. Carried as a `parse_tz.c` patch under
  `components/php/patches/php/`.
- `examples/date-timezones/` and `examples/date-utc/`: `DateTime` across named timezones with
  DST-aware conversions, and the same language in a UTC-only build.

## [0.1.1] 2026-07-29

### Added
- Optional build-time extensions, selectable from `flash.sh` (a `[y/N]` prompt maps to
  `-DPHP_EXT_<NAME>=ON/OFF`). Designed to grow; PDO/SQLite is the first entry.
- **PDO + SQLite** as an optional extension (off by default): `ext/pdo` + `ext/pdo_sqlite`
  built against the SQLite amalgamation, tuned for a single-process, no-OS target on FATFS.
- `scripts/fetch-sqlite.sh`: downloads the SQLite amalgamation on demand (sha256-checked,
  git-ignored), fetched automatically by `flash.sh` when the extension is enabled.
- `examples/sqlite-notes/`: PDO opens a SQLite database on the microSD and appends a row on
  every boot; the file is created on first run and reused after.
- `docs/footprint.md`: flash and RAM usage, per area and per optional extension.

## [0.1.0] - 2026-07-29

First working version: the real PHP engine runs on the microcontroller.

### Language and ecosystem
- The full PHP 8.3 language: namespaces, classes, interfaces, traits, enums, closures and
  arrow functions, generators, `match`, exceptions, typed properties, attributes.
- Standard library: strings, arrays, math, JSON, PCRE regular expressions, hashing, SPL,
  Reflection, random.
- Multi-file programs with `require` / `require_once`.
- **Composer autoloading** (PSR-4 and classmap) with unmodified third-party packages from
  Packagist — verified with Illuminate Collections.
- Not included: `ext/date` (`DateTime`; a minimal stub covers the few core call sites that
  need it — use `hrtime()` for timing), Fibers (no 32-bit RISC-V context-switch assembly),
  `mbstring` / `ctype`, and networking or processes (this hardware has neither).

### Added
- Real **PHP 8.3.32** (Zend engine, `embed` SAPI) running natively on the Waveshare
  ESP32-P4-Pico, executing an `index.php` read from a microSD — no recompiling to change the
  script.
- Bundled extensions: `ext/standard`, PCRE, hash, JSON, SPL, Reflection, random.
- `php_ext_gpio`: a small native extension exposing `gpio_mode`, `gpio_write`, `gpio_read`
  and `delay`, so PHP can drive pins.
- The Arduino-style `setup()`/`loop()` model, with the loop driven from C (periodic GC,
  watchdog, and `zend_try`/`zend_catch` so a script's fatal error doesn't take the board
  down).
- FAT long-filename support, so deep trees like a Composer `vendor/` load from the card.
- Automation scripts: `setup.sh`, `flash.sh`, `monitor.sh`, and `scripts/fetch-php.sh`.
- Examples: `hello`, `language-tour`, `require-demo`, `composer-collections`, `led-blink`,
  `blink-sos`, `button-led`.
- Documentation: README, plus `docs/architecture.md`, `docs/porting-notes.md`,
  `docs/flash.md`.
- MIT license.

### Fixed
- Heap corruption when tearing down scope-bound closures (heavily used by frameworks such as
  Illuminate): the per-closure `efree()` of the run-time cache in `destroy_op_array()`
  corrupted the PSRAM heap. Carried as a patch under `components/php/patches/`, applied by
  `fetch-php.sh`, that lets the request arena own that cache instead.

### Porting decisions
- Hand-written `php_config.h` instead of `./configure`, with the type sizes corrected for
  32-bit RISC-V (ILP32).
- The portable "call" VM instead of the computed-goto "hybrid" one, which doesn't compile on
  RISC-V.
- Zend's memory manager set aside (`USE_ZEND_ALLOC=0`); PHP allocates via `malloc`, routed
  into the 32 MB PSRAM so internal RAM stays free for DMA and FreeRTOS.
- Weak POSIX stubs for the symbols newlib doesn't provide; `ext/date` replaced by a minimal
  stub (its builtin timezone database is ~350 KB).
- microSD powered by the chip's on-chip LDO (channel 4); console over the onboard CH343P
  USB-UART bridge; chip revisions below v3.0 unlocked in `sdkconfig.defaults`.
