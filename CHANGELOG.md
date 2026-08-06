# Changelog

## [0.10.0] — OPcache; configurable CPU frequency

### Added
- **Zend OPcache**, a real port of the bundled extension (`-DPHP_EXT_OPCACHE=ON`), so a request no
  longer recompiles the framework every time — it reuses cached bytecode. Built **without JIT**
  (unsupported on RISC-V) and statically linked (no `opcache.so`/`dlopen` on this target). On the
  ESP32-P4 it takes the [`laravel-demo-optimized`](examples/laravel-demo-optimized/) welcome page
  from ~12 s to **~8.4 s** per request, verified on hardware. See [`docs/opcache.md`](docs/opcache.md).
  - Default **file-cache** mode: bytecode is cached on the microSD, leaving the full PSRAM for the
    per-request heap — the right choice for a large framework.
  - Opt-in **in-memory** mode (`[extensions.opcache] in_memory = true`): the cache lives in PSRAM
    (a heap-backed shared-memory segment) and is served straight from RAM. Faster, but only for a
    **small** app — Laravel's bytecode plus its per-request heap exceed the 32 MB PSRAM, so it stays
    on the file cache.
  - Porting bits: run the `zend_extension` startup by hand (patch `0006`, since `dlopen` extensions
    aren't supported), teach `accel_find_sapi()` the `embed` SAPI, `opcache.file_update_protection=0`
    (the board has no RTC, so files look "from the future"), stub headers + weak POSIX symbols for
    the parts picolibc lacks, and a PSRAM shared-memory backend with no-op locking (patch `0007`).
- **Configurable CPU frequency** — `[board] cpu_freq_mhz` (flash-tool passes `-DPHP_CPU_FREQ_MHZ`,
  layered last over the board's sdkconfig). Note: 400 MHz on an ESP32-P4 **rev < 3.0** is
  experimental and can boot-loop unqualified chips (Espressif); the P4 default (360 MHz) stands
  unless set.

## [0.9.0] — web-server SAPI; Laravel browsable over HTTP

### Added
- **A full per-request SAPI for the `web-server` project type.** The HTTP server in front of PHP now
  turns each request into a complete CGI-style environment and turns the script's response back into
  real HTTP — enough to drive a framework as a browsable app:
  - **Request** — method, full request-URI, split query string, `Host`, `Cookie`, `Content-Type`,
    `User-Agent`, `Accept`, `Referer`, `X-Requested-With`, `Authorization`, and the peer address feed
    a complete `$_SERVER` (via `register_server_variables`); the `Cookie` header populates `$_COOKIE`
    (`read_cookies`); the POST body populates `$_POST` / `php://input` (`read_post`).
  - **Response** — the status code, headers, and cookies the script sets (`Content-Type`, `Location`,
    `Set-Cookie`, …) are translated into the httpd response (`send_headers`), not a hardcoded
    `200 text/html`. Every HTTP method is routed to PHP, which does the real routing.
  - **Static files** — a `GET`/`HEAD` for an existing file under the document root (the entry
    script's directory, e.g. `public/`) is served straight from the httpd task with a by-extension
    `Content-Type` and no framework boot — like `try_files $uri /index.php`. On hardware
    `GET /robots.txt` returns the raw file in ~5 ms, versus ~15–30 s to boot the framework for a
    route. `.php` files and `..` are never served this way; a missing file falls through to PHP.
- **Vanilla Laravel 13 runs as a browsable web app on the ESP32-P4**, verified on hardware over HTTP:
  `GET /` returns `200` with the full welcome page plus Laravel's headers and session cookies
  (`Set-Cookie: XSRF-TOKEN=…`, `laravel-session=…`), and an unknown route returns Laravel's own
  `404`. The [`laravel-demo`](examples/laravel-demo/) example now uses `type = "web-server"`.
- A [`laravel-demo-optimized`](examples/laravel-demo-optimized/) example — the same app tuned to boot
  faster on the board without opcache: authoritative classmap autoloader (far fewer FATFS lookups),
  `--no-dev`, and in-memory session/cache + null log drivers (no per-request card I/O). Deliberately
  skips `config`/`route`/`view` caching, which bakes host-absolute paths and isn't portable from the
  build PC to the card's `/sdcard`.

## [0.8.0] — Laravel runs; FATFS path fixes

### Added
- **Vanilla Laravel** (unmodified `laravel/laravel`, Laravel 13) renders its welcome page on the
  ESP32-P4 — the [`laravel-demo`](examples/laravel-demo/) example. The whole framework runs from the
  microSD (`vendor/` and all), verified on hardware.
- **Configurable entry script** — `[php] entry` / `-DPHP_ENTRY` (default `index.php`), so a framework
  with a nested front controller runs (`public/index.php`). flash-tool passes it via `build.EntryArg`.
- A **minimal `$_SERVER`** (a `GET /` request) is set for the run-once (init-loop) model, so a
  framework front controller captures a sane request instead of guessing from empty globals.

### Fixed
- **FATFS paths with `.` / `..`.** The ESP-IDF FATFS VFS doesn't resolve `.`/`..` path components, so
  code that builds relative paths — Laravel's `__DIR__."/../../../../config"` — failed `is_dir()`.
  `main/fs_pathnorm.c` `--wrap`s the file syscalls (`stat`/`lstat`/`open`/`opendir`/`access`/`mkdir`/
  `unlink`/`rmdir`/`rename`) and normalizes paths before the VFS sees them.
- **`lstat()` / `realpath()` on FATFS.** FATFS's `lstat` is unimplemented, which made PHP's
  `realpath()` (it `lstat`s every path component) return `false` for everything — breaking any code
  that resolves paths. Since FAT has no symlinks, `lstat` is now routed to `stat`.

## [0.7.0] — session and tokenizer extensions

### Added
- **`ext/tokenizer`** (`-DPHP_EXT_TOKENIZER=ON`, ~13 KB): `token_get_all()`, `token_name()`, the
  `PhpToken` class and the `T_*` constants, over the engine's own lexer. Self-contained (no bundled
  data). Verified on hardware — see the [`tokenizer-demo`](examples/tokenizer-demo/) example.
- **`ext/session`** (`-DPHP_EXT_SESSION=ON`, ~50 KB): `session_start()`, `$_SESSION`, `session_id()`,
  with the default **files** save handler (point `session.save_path` at a writable dir — the
  microSD) and the user save handler (`session_set_save_handler`). Sessions persist across reboots
  on the card. Verified on hardware — see the [`session-demo`](examples/session-demo/) example.
- Patch `0005-session-files-no-cloexec-warn.patch`: the files handler's `fcntl(F_SETFD, FD_CLOEXEC)`
  is meaningless on this target (no fork/exec) and the FATFS VFS rejects it with `EINVAL`; the patch
  attempts it without warning, so `session_start()` stays quiet.

### Changed
- The firmware prints its boot banners (`PHP … on …`, `--- <script> ---`, `--- end ---`) with plain
  `printf` instead of `php_printf`, and clears `SG(headers_sent)` before running the script in the
  init-loop model. The embed SAPI marks headers sent at startup (it's a CLI-like, no-HTTP SAPI),
  which otherwise makes `session_start()` / `setcookie()` / `header()` and the session ini settings
  refuse with "headers already sent". Console output is unchanged. (The web-server model keeps the
  flag — it sends real HTTP headers.)

## [0.6.0] — TLS client (HTTPS) and static DNS

### Added
- **TLS client / HTTPS** for the full openssl build, via a new **`tls`** setting
  (`[extensions.openssl] tls = true` → `-DPHP_EXT_OPENSSL_TLS=ON`). openssl.c registers the
  `ssl://` / `tls://` stream transports but its own TLS layer (`xp_ssl.c` + libssl) can't be built on
  this target; instead the transport factory is implemented on ESP-IDF's **esp-tls / mbedTLS**
  (`components/php/compat/openssl_tls_esptls.c`), which does DNS + TCP + TLS handshake in one call.
  So PHP's normal stream layer reaches HTTPS: `file_get_contents('https://…')`,
  `stream_socket_client('tls://host:443')`. The crypto stays real OpenSSL 3.0 (libcrypto); only the
  TLS record layer rides mbedTLS. **Client only**, needs a networked board. Verified on real
  ESP32-P4-ETH: a certificate-verified GET of `https://example.com/`. See the
  [`https-client`](examples/https-client/) example and `docs/openssl.md`.
- **Certificate provisioning.** When a project builds the TLS client, `phpflash build` copies the
  **host's** root-CA bundle into the project (`project-src/certs/ca-bundle.crt` by default) so it
  ships to the device, and the firmware verifies TLS peers against it. Configurable with
  `[extensions.openssl] certs_path` (destination) and `certs_source` (host bundle; auto-detected from
  the usual Fedora/Debian/… locations otherwise). Git-ignored in examples. `phpflash build` ships it
  once but won't overwrite it; the new **`phpflash update-certs`** command refreshes it (re-copies
  the host trust store into `certs_path`), for renewed roots or a changed `certs_source`.
- **Static DNS** — `[network] dns = ["1.1.1.1", "8.8.8.8"]`. The firmware applies these to the netif
  after the DHCP lease (so they win over DHCP's); empty keeps the DHCP-provided servers. Passed as
  `-DPHP_NET_DNS` (comma-separated).
- **`https-client` example** (ESP32-P4-ETH): a certificate-verified HTTPS GET from PHP.

### Changed
- The base config now uses **software AES/GCM in mbedTLS** (`CONFIG_MBEDTLS_HARDWARE_AES=n`,
  `…_GCM=n`). The PHP heap is PSRAM, and the hardware AES accelerator's DMA can't reach PSRAM, which
  made the TLS handshake fail to allocate DMA descriptors. Software AES works from any memory (a TLS
  handshake takes ~10 s — software RSA verify + AES — which the widened watchdog covers).

## [0.5.0] — the `openssl` extension (mbedTLS subset + real OpenSSL)

### Added
- **`openssl` extension**, in two flavours a project picks between (`[extensions.openssl]`, off by
  default):
  - **Compatible subset** (`-DPHP_EXT_OPENSSL=ON`, ~42 KB) — a hand-written extension backed by
    ESP-IDF's **mbedTLS**, providing the symmetric-cipher functions: `openssl_encrypt` /
    `openssl_decrypt` (AES-128/192/256-CBC and -GCM), `openssl_cipher_iv_length`,
    `openssl_random_pseudo_bytes` (hardware RNG), `openssl_error_string`, and the `OPENSSL_RAW_DATA`
    / `OPENSSL_ZERO_PADDING` constants. Byte-for-byte interoperable with desktop OpenSSL; enough for
    e.g. a framework's encrypter. No RSA/X.509/TLS.
  - **Full** (`… -DPHP_EXT_OPENSSL_FULL=ON`, ~2.1 MB) — the *real* `ext/openssl` compiled against a
    **ported OpenSSL 3.0 libcrypto**, cross-compiled for the chip by `scripts/fetch-openssl.sh`
    (static, no-PIC, hardware-RNG seed, a one-line `<syslog.h>` shim). The full crypto API: RSA/EC
    keys, `openssl_sign`/`verify`, `openssl_public_encrypt`/`private_decrypt`, `openssl_digest`
    (SHA-2/3, RIPEMD, …), X.509/PKCS parsing. Crypto only — no TLS stream transport (`ssl://`), which
    would need `libssl`. Both flavours verified on real ESP32-P4 hardware.
  - Choose via the config (`[extensions.openssl] full = true|false`); `phpflash` and `flash.sh` pass
    the flags and run the fetch. `docs/openssl.md` explains when to use which.
  - **On-chip RSA key generation** (`openssl_pkey_new`) works in the full build. OpenSSL 3.0 needs a
    config file to bring its providers up, so the firmware reads an `openssl.cnf` shipped with the
    source (`phpflash build` writes a minimal one into `project-src/`, and the firmware sets
    `OPENSSL_CONF` to it). Verified on hardware: RSA-2048 generation + sign/verify. It's CPU-bound
    (~20-45 s, probabilistic), so the base config raises the task-watchdog timeout to 60 s
    (`CONFIG_ESP_TASK_WDT_TIMEOUT_S`). EC keygen and `openssl_csr_sign`/X.509 issuing remain untested
    on this port.
  - **`[extensions.openssl] config_path`** — override where the `openssl.cnf` lives (relative to the
    source folder, or an absolute on-device path). Passed to the firmware as `-DPHP_OPENSSL_CONF`.
  - **`no_load_config` setting** (`-DPHP_EXT_OPENSSL_NO_LOAD_CONFIG=ON`, off by default) — build the
    full openssl to skip the config file entirely (`OPENSSL_INIT_NO_LOAD_CONFIG`): leaner, for
    devices that only *use* provisioned keys and never generate them on-chip.
- Examples: **`openssl-compat`** (AES via the subset) and **`openssl-full`** (RSA sign/verify +
  digests + on-chip keygen via real OpenSSL).

## [0.4.0] — ESP32-P4-ETH board, Ethernet and the web-server project type

### Added
- **ESP32-P4-ETH board** (`boards/esp32-p4/esp32-p4-eth/`), the second board on the ESP32-P4
  family. It shares the P4 SD reference design with the Pico — the same 4-bit SDMMC microSD on
  GPIO39-44, powered by the on-chip LDO (channel 4) — so its `board.c` mounts storage the same
  way, with one addition: the card's VDD is gated by a high-side P-MOSFET driven from GPIO45,
  which the board drives on before mounting (it defaults on via a pulldown, so this just makes it
  deterministic). Console is UART0 over the on-board CH343P USB-UART bridge, same single-cable
  flashing/monitor as the Pico. Select it with `-DBOARD=esp32-p4-eth`. Verified on real hardware.
- **Ethernet bring-up.** A board that declares `BOARD_HAS_NETWORK` (the ETH board) brings its
  network up at boot and logs the address — `board_network_up()` in the board's `board.c`
  initialises the IP101 RMII PHY, runs a DHCP client and returns the IPv4 address, which `main.c`
  logs as `network up -- http://<ip>/`. The ESP32-P4 EMAC default pin map matches this board's
  wiring exactly. The Pico (no network) skips all of it. Verified end-to-end (real DHCP lease).
- **`web-server` project type** — a second firmware execution model, selected with
  `-DPHP_PROJECT_WEB_SERVER=ON` (which `phpflash` passes when a project's `type = "web-server"`).
  Instead of the run-script + `setup()`/`loop()` model, an HTTP server (`esp_http_server`) runs in
  front and PHP is invoked **fresh per request** — shared-nothing, the way a script runs behind
  Apache/PHP-FPM. The firmware captures the script's output as the response body and gives it a
  minimal `$_SERVER` (method, URI). PHP runs in the main task (with its large stack); the httpd
  task just hands off each request. Now marked available in the manifest (with its build flag),
  and advertised by the ETH board. Verified on hardware (page served to a browser).
- Two examples: **`web-server-init-loop`** (the whole HTTP server written in PHP with
  `stream_socket_server`, in the init-loop model — stateful across requests) and **`web-server`**
  (the `web-server` project type — shared-nothing per request). Both need the ETH board.
- **microSD support is now optional** (`-DPHP_STORAGE_MICROSD`, on by default). An `embedded`
  project can turn it off to build a self-contained firmware that runs from internal flash with no
  card — dropping the SDMMC drivers and the board's SD mount code (~51 KB) and skipping the mount
  at boot. Also lets a board that has no card slot build cleanly. `phpflash` sets it from the
  project: `microsd` storage → on; `embedded` → off unless the config opts in with
  `[storage] microsd = true` (also mount a card for writable data). The board's `board.cmake`
  pulls the SD drivers only when it's on.

### Changed
- **PHP's CSPRNG now uses the ESP32 hardware RNG.** `ext/random`'s `csprng.c` only reached for
  `getrandom()` on Linux/BSD and otherwise fell straight to `/dev/urandom`, which doesn't exist
  here — so `random_bytes()`, `random_int()`, `session_id()` etc. threw `Random\RandomException`.
  A patch (`patches/php/0004-csprng-esp-getrandom.patch`) enables the `getrandom()` path whenever
  the libc provides it; on ESP-IDF newlib backs `getrandom()` with `esp_fill_random` (the hardware
  RNG). PHP's random functions now work. Verified on hardware.
- Fixed board selection in ESP-IDF's early requirement-expansion phase, which doesn't see
  `-DBOARD`: a non-default board previously inherited the **default** board's component
  requirements (latent until the ETH board needed networking components the Pico doesn't). The
  top-level `CMakeLists` now exports the resolved board (and the web-server flag) into the
  environment, which `resolve-board.cmake` / `main/CMakeLists` read as a fallback.

## [0.3.0] — embedded storage mode

### Added
- **Embedded storage mode.** The PHP source can now live inside the chip instead of on a microSD,
  for boards that ship without a card. A fourth partition, `storage` (8 MB, `fat`), holds a
  read-only FAT image built from a source directory when the build is given `-DPHP_EMBED_SRC=<dir>`
  (via `fatfs_create_rawflash_image`, with long-filename support so a Composer `vendor/` fits);
  `idf.py flash` writes it alongside the app. At boot the firmware mounts the embedded image at
  `/app` and runs `/app/index.php` if present, otherwise falls back to `/sdcard/index.php`. The two
  are not exclusive: a microSD, when present, is still mounted read-write at `/sdcard`, so an
  embedded project can keep writable data (a SQLite database, logs) on the card. Without
  `PHP_EMBED_SRC` the partition is left empty and the firmware behaves exactly as before (runs from
  the card). Verified on real ESP32-P4-Pico hardware: with a card inserted, the board still ran the
  embedded `/app/index.php` from internal flash.
- `storage`, an 8 MB `fat` partition, in the ESP32-P4-Pico partition table — empty unless a build
  supplies `PHP_EMBED_SRC`.
- `storage_type = "embedded"` is now marked available in the version manifest (was reserved);
  `scripts/info.sh` lists the board's storage as `microsd, embedded`.

### Changed
- `main/main.c` now mounts both storage sources at boot (microSD at `/sdcard`, the embedded image
  at `/app`) and picks the script by priority (`/app` over `/sdcard`), instead of only running
  `/sdcard/index.php`. The `main` component gains the `esp_partition fatfs vfs` requirements.

## [0.2.0] — multiple PHP versions, boards and chip families

### Added
- **Support for multiple boards and chip families.** Everything specific to a board now lives
  under `boards/<family>/<board>/` (for now `boards/esp32-p4/esp32-p4-pico/`): its config
  (`sdkconfig.board`, `partitions.csv`), its pins, and its **code** — `board.c` implements a
  small interface (`board.h`: `board_mount_storage()`/`board_unmount_storage()`), so the microSD
  wiring lives with the board and `main/main.c` is board-agnostic. Chip-family settings (ESP-IDF
  target, PSRAM) sit in `boards/<family>/sdkconfig.family`. `sdkconfig` is layered base → family
  → board; the top-level `CMakeLists.txt` selects the board (`default_board` in `php-esp32.toml`,
  override `-DBOARD=<board>`). Adding a board or a whole family is a new directory. Each board
  also carries a `board.toml` declaring which **storage types** (`microsd`/`embedded`) and
  **project types** (`init-loop`/`web-server`/`event-driven`) its hardware supports; the version
  manifest declares which are implemented, and the two combine (e.g. no `web-server` on the Pico,
  which has no wired network).
- **Support for multiple PHP versions.** Everything version-specific now lives under
  `components/php/versions/<version>/`: the hand-written config headers, the source list
  (`sources.cmake`), the optional-extension wiring (`extensions.cmake`), the patches
  (`patches/php/`), the version-sensitive `compat/` files (`date_stub.c`, `timelib_config.h`,
  `timezonedb_minimal.h`), and the tarball coordinates (`version.env`). `components/php/CMakeLists.txt`
  is now generic and selects `PHP_VERSION` (default from the new root `php-esp32.toml`; override
  with `-DPHP_VERSION=<ver>`); `scripts/fetch-php.sh` reads the chosen version's `version.env` and
  patches. Adding a PHP version is a new directory, not edits across the build. The 8.3.32 build
  is unchanged.
- A machine-readable **extension manifest** per version
  (`components/php/versions/<version>/manifest.toml`) — the contract the `flash-tool` CLI reads
  (build flags, settings, dependencies, fetch scripts, per-project-type rules) — with
  `scripts/check-manifest.py` verifying it stays in sync with that version's `extensions.cmake`
  and `flash.sh`.
- `docs/ext-porting.md`: the status of every PHP 8.3 bundled extension — built-in, behind a
  build flag, or not ported (with the reason).
- `scripts/info.sh`: prints what a checkout can build — the default version/board, the available
  PHP versions and boards, and per board the modes it offers (implemented ∩ board-supported).
  Also the reference for how `flash-tool` discovers the repo.
- Add-a-version / add-a-board HOWTOs (`components/php/versions/README.md`, `boards/README.md`),
  a family descriptor (`boards/<family>/family.toml`), and `check-manifest.py` now also validates
  the board/family descriptors.

### Changed
- Dropped `CONFIG_SPIRAM_IGNORE_NOTFOUND` now that PSRAM is proven stable, so a real PSRAM
  failure panics loudly instead of degrading silently.

## [0.1.3] — ctype / mbstring / filter + onugiruma (rejex)

### Added
- **`ext/ctype`**, **`ext/mbstring`** and **`ext/filter`** as optional extensions (off by
  default): three more of PHP's bundled extensions ported to the target — character-class
  checks, multibyte strings and `filter_var()` validation/sanitization. Measured flash cost:
  ctype ~2.5 KB, filter ~27 KB, mbstring ~965 KB (see `docs/footprint.md`).
- `mbstring` is built without oniguruma by default, so the `mb_ereg*`/`mb_split` regex family is
  left out; everything else (length, case, `substr`, `convert_encoding`, `detect_encoding`,
  `str_split`, …) is in.
- `PHP_EXT_MBSTRING_ONIG`: a sub-option (asked by `flash.sh` when `mbstring` is on) that builds
  the `mb_ereg*`/`mb_split` regex family for real, vendoring the oniguruma library via
  `scripts/fetch-oniguruma.sh` (sha256-checked, git-ignored). Adds ~445 KB.
- `PHP_EXT_MBSTRING_NO_CJK`: a sub-option (asked by `flash.sh` when `mbstring` is on) that drops
  the legacy CJK codecs (Shift-JIS, EUC-*, Big5, GB18030, `mb_convert_kana`), taking mbstring
  from ~965 KB to ~209 KB. UTF-8/UTF-16/Latin unaffected. Carried as a patch under
  `components/php/patches/php/` (a no-op unless the macro is set).
- `examples/ctype-demo/`, `examples/mbstring-demo/`, `examples/mbstring-no-cjk/`,
  `examples/filter-demo/`: one runnable example per extension (plus the CJK-dropped variant),
  each selectable independently in `flash.sh`.
- `examples/eloquent-demo/` and `examples/eloquent-onig/`: Laravel's Eloquent ORM running
  standalone (no framework), reading and writing a SQLite database on the microSD — a `Post`
  model with the schema builder, query builder and Carbon timestamps. Exercises the whole stack
  at once (`pdo`/`sqlite` + `mbstring` + `ctype` + `filter` + `date`). The `-demo` variant runs
  on an mbstring built without oniguruma (it polyfills `mb_split` over PCRE); `-onig` runs on the
  oniguruma build, where `mb_split` is native.
- `examples/mbstring-regex/`: the `mb_ereg*`/`mb_split` family on the Oniguruma engine, with a
  Unicode-property pattern (`\p{L}`).

### Porting
- `ext/ctype` guards its whole body with `#ifdef HAVE_CTYPE`; the hand-written `php_config.h`
  leaves it undefined, so `HAVE_CTYPE` is defined for that one file (scoped) to avoid an
  `undefined reference to ctype_module_entry` at link.
- `ext/mbstring` needs `libmbfl/config.h` (normally generated by `configure`); it's generated as
  a one-line shim in the build tree that pulls in `php_config.h`.

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
