# Changelog

## [0.17.0] — WIP — WiFi from PHP: scan, join, or create a network, and serve web pages over it

### Added
- **`wifi`: scan/join a WiFi network and create one, from PHP.** An opt-in native extension for
  **WiFi-capable SoCs** (ESP32 / ESP32-S3 / C-series -- **not** the radio-less ESP32-P4, which fails
  the build early with a clear message). Client (STA): `wifi_scan()` lists the nearby APs
  (`ssid`/`bssid`/`rssi`/`channel`/`auth`), `wifi_connect($ssid, $password = null)` joins one (open,
  WPA2, or WPA2/WPA3-transition -- it is PMF-capable, does WPA3-SAE, scans every channel, and
  auto-retries transient association drops so real phone hotspots connect reliably), plus `wifi_ip()` /
  `wifi_rssi()` / `wifi_connected()` / `wifi_disconnect()`. Access point
  (SoftAP): `wifi_ap_start($ssid, $password = null, $channel, $max_conn)` makes the board its own
  network with a built-in DHCP server (default 192.168.4.1), plus `wifi_ap_ip()` /
  `wifi_ap_clients()` / `wifi_ap_stop()`. Driven straight from `esp_wifi`; credentials are never baked
  into the config -- PHP passes them at runtime. Registered across every PHP version (8.3-8.5); ~600 KB
  of flash, so opt-in (`[extensions.wifi] enabled = true`). Verified on ESP32-S3-Zero hardware (scanned
  real networks, joined a WPA2/WPA3 network and got a DHCP lease, and brought up a SoftAP). New
  [`wifi-connect`](examples/wifi-connect/) (credentials come from a gitignored `.env` baked into the
  firmware) and [`wifi-ap`](examples/wifi-ap/) examples.
- **The `web-server` model now runs on WiFi-only ESP32-S3 boards.** Every ESP32-S3 has WiFi on the
  die, so all S3 board profiles (`esp32-s3-zero`, `esp32-s3-pico`, `esp32-s3-eth`) now advertise the
  `web-server` project type -- previously reserved for boards with wired Ethernet. The HTTP server was
  always independent of the wired-network path; the network simply comes from the `wifi` extension
  instead. A `[web-server] init` (server_init) script brings WiFi up before the server binds, so a
  board can create its own network and serve pages over it with no router or cable. New
  [`wifi-ap-s3-rgb-manage`](examples/wifi-ap-s3-rgb-manage/) example: the board starts a SoftAP and
  serves a live web page (from PHP) that controls the onboard RGB LED's colour and brightness --
  verified end to end on ESP32-S3-Zero hardware (AP up, page served over HTTP on the WiFi, LED reacts
  live to the sliders). (`$_SERVER['SERVER_ADDR']` reads `0.0.0.0` on WiFi-only boards -- use
  `wifi_ap_ip()` / `wifi_ip()` for the real address.)

## [0.16.0] — Onboard RGB LED from PHP, per-project partition tables, and the real ESP32-S3-Zero

### Added
- **`s3_onboard_rgb`: drive the ESP32-S3 board's onboard WS2812 RGB LED from PHP.** An opt-in native
  extension, **ESP32-S3 only** -- the P4 boards have no such LED. Enable it per project with
  `[extensions.s3_onboard_rgb] enabled = true`; the data pin is configurable (`pin`, default GPIO 48)
  because S3 boards wire the LED to different pins. It drives the WS2812 straight from the SoC's RMT
  peripheral (no external component). API: `s3_onboard_rgb_set($r, $g, $b)`,
  `s3_onboard_rgb_hsv($h, $s, $v)`, `s3_onboard_rgb_off()`, `s3_onboard_rgb_available()`, plus the
  `S3_ONBOARD_RGB_PIN` constant. Compiled in only for `esp32s3` targets: enabling the flag on a P4
  board fails the build early with a clear message (`PHP_EXT_S3_ONBOARD_RGB is ESP32-S3 only`).
  Registered across every PHP version (8.3-8.5). Verified on ESP32-S3-Zero hardware (GPIO 48). New
  [`s3-rgb-show`](examples/s3-rgb-show/) example: a slow, continuous rainbow on the onboard LED.
- **Per-project partition table.** A project can now ship its own `partitions.csv` next to
  `php-esp32.config.toml`; for that build it replaces the board's committed fixed spec.
  `cmake/gen-partitions.cmake` takes it via a new `PHP_PARTITIONS_CSV` input (flash-tool passes it
  when the file is present) and still appends the generated `storage`/`phpstore` partitions. This lets
  one project resize `factory` or add a partition on a tight-flash board without forking the firmware.
  New [`custom-partitions`](examples/custom-partitions/) example and a new reference page,
  [docs/reference/partitions.md](docs/reference/partitions.md).

### Fixed
- **`esp32-s3-zero` corrected to the real hardware.** Its definition inherited the S3-ETH module's
  assumptions (16 MB flash, Octal 8 MB PSRAM), but the actual Waveshare ESP32-S3-Zero (ESP32-S3FH4R2)
  carries **4 MB flash and 2 MB Quad PSRAM**. `sdkconfig.board` now pins 4 MB flash and overrides PSRAM
  to **Quad** (the family default is Octal -- in Octal mode a Quad module never initialises, and with
  `USE_ZEND_ALLOC=0` the whole runtime heap lives in PSRAM, so the engine would have no memory to run
  in), and `partitions.csv` caps `factory` at 3456K so the ~2.8 MB firmware plus its embedded FAT fit
  in 4 MB. Verified on ESP32-S3-Zero hardware: `hello` runs from an embedded image.

## [0.15.0] — Sharing state across web-server requests: `mem_*` and a server-init script

### Added
- **A volatile in-RAM key-value store for PHP (`mem_*`).** The RAM twin of `store_*`: `mem_set()` in
  one request comes back with `mem_get()` in the next, shared across the requests of one boot. It
  lives in persistent (non-request) memory so it survives `php_request_shutdown()`, is wiped on
  reboot, and touches no flash -- so, unlike the NVS-backed `store_*`, it is fine to write on every
  request (a hit counter, a small cache). Values are stored with PHP's serializer, so scalars,
  arrays and serializable objects work; each `mem_get()` returns an independent copy. Always built
  in, no configuration. `mem_set/get/has/delete/clear/keys`. Verified on ESP32-P4 hardware (scalars,
  nested arrays and a serializable object round-trip), and end-to-end over HTTP on the ESP32-P4-ETH:
  a counter kept in `mem_*` climbs across separate requests. See [docs/storage/in-ram-store.md](docs/storage/in-ram-store.md).
- **A one-time init script for the web-server model (`[web-server] init`).** A project names a PHP
  file that the firmware runs **once**, after the engine starts and before the HTTP server accepts
  connections, for setup shared across the (otherwise shared-nothing) requests -- bringing hardware
  up through a C extension, or seeding `mem_*` / `store_*`. Its output goes to the console; a fatal
  in it is logged but does not stop the server. flash-tool passes it as `-DPHP_WEB_INIT`; the
  firmware resolves it against the source mount and runs it in the embed request before redirecting
  output to the HTTP response. Verified end-to-end on the ESP32-P4-ETH: the init script runs once at
  boot and the value it seeds is read by every subsequent HTTP request. New
  [`web-init-mem`](examples/web-init-mem/) example. See [docs/storage/in-ram-store.md](docs/storage/in-ram-store.md).

## [0.14.0] — Slimmer board variants: network-less (`-pico`) and embedded-only (`-zero`)

### Added
- **A new board: `esp32-s3-pico`** -- a plain ESP32-S3 with a microSD slot and no wired network. It
  is the S3-ETH without the W5500: the same microSD-over-SPI wiring (MOSI=6, MISO=5, CLK=7, CS=4), 8
  MB PSRAM and 16 MB flash, but no Ethernet stack and no `web-server` model (`init-loop` and
  `event-driven` only). Verified running PHP 8.4.24 from an embedded image on ESP32-S3 hardware, and
  the microSD path builds clean.
- **Two embedded-only boards: `esp32-s3-zero` and `esp32-p4-zero`.** Each is the family's `-pico`
  stripped further -- no microSD slot at all -- so they advertise only `embedded` storage: the PHP
  source is packed into a read-only FAT image in flash. No network, so `init-loop` and `event-driven`
  only. "No microSD" is a build-time contract: forcing the card path on (`[storage] microsd = true`)
  stops the build with a clear message rather than compiling SD code for pins that aren't wired.
  Both verified running PHP 8.4.24 from flash -- `esp32-s3-zero` on ESP32-S3 hardware and
  `esp32-p4-zero` on ESP32-P4 hardware.
- **A board capability macro `BOARD_HAS_MICROSD`** (mirroring `BOARD_HAS_NETWORK`). Boards with a
  card slot define it in `board.h`; the embedded-only `-zero` boards leave it undefined. `main.c`
  centralises the "no card slot" contract on it (one guard for every board), and the `discover --all`
  probe firmware skips the microSD probe (reporting `microsd=n/a`) on boards without a slot instead
  of failing to build.

## [0.13.0] — Dynamic partitions, build-time environment (`.env`), and a persistent store

### Added
- **A reboot-persistent key-value store for PHP (`store_*`), backed by NVS.** A new built-in
  extension: `store_set()` writes a value that survives a reset and comes back with `store_get()` on
  the next boot; also `store_has`, `store_delete`, `store_clear`, `store_keys` and `store_available`.
  It is off until the project gives it flash with `[store] size_kb`, which the dynamic partition
  generator turns into a dedicated `phpstore` NVS partition. Keys are ≤15 chars, values are strings
  (auto-committed). Verified on the ESP32-P4 with the [`store-demo`](examples/store-demo/) boot
  counter. See [docs/storage/persistent-store.md](docs/storage/persistent-store.md).
- **A project `.env` is baked into the firmware and exposed to PHP as `$_ENV` and `getenv()`.** Drop a
  `.env` next to `php-esp32.config.toml` (`KEY=VALUE`, `#` comments, optional quotes) and its values
  are compiled in; `main.c` applies them with `setenv()` before the engine starts, so PHP's normal
  environment import fills `$_ENV` (the build's `variables_order` carries `E`) and `getenv()` returns
  them. Works in both the init-loop and web-server models. Configurable from the project config
  (`[env] enabled` / `file`); on by default when a `.env` exists, and `phpflash init` adds `.env` to
  the project `.gitignore`. Verified on the ESP32-P4 with the [`env-demo`](examples/env-demo/)
  example. See [docs/storage/environment.md](docs/storage/environment.md).
  - The values live in the app image in internal flash, **not** on the removable microSD and not in
    the PHP source tree. They are not secret or encrypted (a flash dump recovers them), but they are
    not readable by pulling the card and are harder to extract than a file on it.

### Changed
- **The flash partition layout is now generated per build** (`cmake/gen-partitions.cmake`) instead of
  a fixed CSV. A board's `partitions.csv` keeps only the constant partitions (`nvs`, `phy_init`,
  `factory`); the `storage` partition -- the read-only FAT image that holds an embedded project's PHP
  source -- is generated:
  - **`embedded` project**: `storage` is sized to the source (cluster-rounded occupancy + FAT
    overhead + the configurable `[storage] reserve_kb`, aligned to 64 KB, 128 KB floor) rather than a
    fixed multi-megabyte block. A tiny sketch now gets a 128 KB partition instead of 8 MB.
  - **`microsd` project**: no `storage` partition at all -- the source runs from the card, and that
    flash stays free (about 20 MB of the P4's 32 MB is left unpartitioned). `main.c` already falls
    back to the card when the partition is absent, so no runtime change was needed.
  - The generated table (`partitions.gen.csv`) is written into the project's `build/` and selected via
    a sdkconfig fragment layered last; it is never committed. Verified on the ESP32-P4-ETH for both an
    embedded build (128 KB storage, source mounts and runs) and a microSD build (no storage
    partition). See [docs/reference/footprint.md](docs/reference/footprint.md).

## [0.12.0] — SSD1306 OLED examples and per-project C extensions; Ethernet optional at boot

### Added
- An [`oled-ssd1306-fps`](examples/oled-ssd1306-fps/) example: a 0.91" SSD1306 128x32 OLED driven
  entirely from PHP. The I2C link is bit-banged over two GPIO pins with the `gpio` extension -- no
  C-side I2C driver, every START, clock edge and byte is a PHP call. It benchmarks full-frame
  throughput (the 512-byte framebuffer, flushed in one transaction) and shows the rate both on the
  panel and on the serial log. On the ESP32-P4-ETH the pure-PHP driver holds **~41 FPS**, verified on
  hardware. The driver is a reusable `SSD1306` class in its own file (`project-src/SSD1306.php`).
- **Custom C extensions per project.** A project can drop PHP extensions written in C under
  `./firmware/exts/<name>/` and phpflash compiles them into the firmware -- no forking the base
  firmware. Each directory is one extension and must define `zend_module_entry <name>_module_entry`
  (the same shape as the built-in `gpio`); its sources are compiled with the engine headers, and the
  firmware registers it at startup via `zend_startup_module()` so its functions are available to the
  script. Extensions are statically linked (there is no `dlopen` on this target). Extra ESP-IDF
  component dependencies go in `firmware/exts/<name>/idf_requires.txt`. See
  [docs/extensions/custom-extensions.md](docs/extensions/custom-extensions.md).
  - New firmware component `php_project_exts` (globs the project's extension dirs, generates the
    registration table, links `WHOLE_ARCHIVE`); a single weak-symbol hook in `main.c`; and phpflash
    passes `-DPHP_PROJECT_EXTS_DIR` when `./firmware/exts` exists. A firmware built without any
    project extensions is unchanged.
- An [`oled-ssd1306-ext`](examples/oled-ssd1306-ext/) example: the same SSD1306 panel driven by a
  **native C extension** (hardware I2C, framebuffer and text in C) instead of the pure-PHP bit-banged
  driver, built with the per-project extension mechanism above. On the ESP32-P4-ETH it runs full
  frames at **~82 FPS** -- twice the pure-PHP driver, now bound by the 400 kHz I2C bus rather than the
  interpreter. Verified on hardware.

### Changed
- **Ethernet is no longer required to boot.** On the networked boards (ESP32-P4-ETH, ESP32-S3-ETH),
  `board_network_up()` now watches the PHY link and, when there is no link (cable unplugged), returns
  at once instead of blocking the full 15 s DHCP timeout before the script runs. A sketch that doesn't
  use the network -- like the OLED demos -- now boots straight into `setup()`/`loop()`. With a cable the
  behaviour is unchanged: link comes up, then it waits for the DHCP lease as before.

## [0.11.0] — Multiple PHP versions (8.3, 8.4, 8.5)

### Added
- **PHP 8.4 (8.4.24) and PHP 8.5 (8.5.9) now build alongside 8.3**, selectable per project with
  `-DPHP_VERSION=<ver>` (or `default_version` in [`php-esp32.toml`](php-esp32.toml); phpflash reads it).
  Every version is a self-contained directory under `components/php/versions/<ver>/` — its own
  `sources.cmake`, config headers, patches and `manifest.toml` — so the three coexist without touching
  the shared engine glue or the boards. All three are verified on the ESP32-P4 with the
  [`patch-test`](examples/patch-test/) suite (all patches PASS on each). See
  [`components/php/versions/README.md`](components/php/versions/README.md).
  - **8.4.24** porting bits: the `SETJMP` selection in `zend_portability.h` switched to
    `#ifndef ZEND_WIN32` upstream (assumes `sigsetjmp`, which picolibc lacks), restored behind
    `HAVE_SIGSETJMP` (new patch `0008`); new Zend sources wired in (`zend_frameless_function.c`,
    `zend_lazy_objects.c`, `zend_property_hooks.c`), `ext/random/zend_utils.c` now feeds the core RNG,
    and `ext/pcre/pcre2lib/pcre2_chkdint.c` is required; a weak `getppid` stub (used in random's
    fallback seed); `mbstring` CJK/mobile codec tables reorganised upstream, patch `0003` regenerated.
  - **8.5.9** porting bits: **`ext/uri` is now a core dependency** (the standard library requires it),
    so it is built in — the RFC 3986 parser (bundled uriparser) and the legacy `parse_url()` parser
    that the stream layer uses. Its **WHATWG backend (lexbor) is dropped**: ~370 KB of static tables
    overflow the ESP32's internal RAM. `parse_url()`, `FILTER_VALIDATE_URL` and `Uri\Rfc3986\Uri` all
    work; `Uri\WhatWg\Url` registers but raises on construction (patch `0009` drops the lexbor module
    dependency, a stub replaces the parser). OPcache's startup was reworked for 8.5 (the extension
    struct went `static`, and `accel_startup` no longer registers the module — done in
    `internal_functions.c` now, patch `0006` updated); the bundled `main/php_glob.c` is compiled with
    `HAVE_REALLOCARRAY` plus weak `getpwuid_r`/`getpwnam_r` stubs.
- **8.3.32 → 8.3.33** — tracked the upstream security release; all seven patches still apply, image and
  behaviour unchanged.
- A [`patch-test`](examples/patch-test/) example: an embedded suite that exercises every port patch at
  runtime (closure runtime-cache arena, `ext/date` timezones, mbstring CJK codecs, the hardware
  CSPRNG, the session files handler, and — with OPcache on — the static-embed and PSRAM-SHM paths) and
  prints PASS/FAIL/SKIP to the serial log. It is the check run against each version bump on hardware.

### Notes
- The baseline image grows with the release, almost entirely in flash: **~3.09 MB** (8.3.33),
  **~3.20 MB** (8.4.24, +105 KB), **~3.29 MB** (8.5.9, +198 KB) for the same all-optional-off build on
  the P4. Static internal RAM is unchanged across the three. See
  [`docs/reference/footprint.md`](docs/reference/footprint.md).

## [0.10.0] — OPcache; configurable CPU frequency

### Added
- **Zend OPcache**, a real port of the bundled extension (`-DPHP_EXT_OPCACHE=ON`), so a request no
  longer recompiles the framework every time — it reuses cached bytecode. Built **without JIT**
  (unsupported on RISC-V) and statically linked (no `opcache.so`/`dlopen` on this target). On the
  ESP32-P4 it takes the [`laravel-demo-optimized`](examples/laravel-demo-optimized/) welcome page
  from ~12 s to **~8.4 s** per request, verified on hardware. See [`docs/extensions/opcache.md`](docs/extensions/opcache.md).
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
- A [`symfony-demo`](examples/symfony-demo/) example — **Symfony 7.4** (skeleton + a controller),
  browsable over HTTP, running the pre-compiled prod container from the microSD at **~2.1 s per
  request** (OPcache file-cache warm), verified on the ESP32-P4. Symfony 8 needs PHP 8.4, so this pins
  7.x (PHP 8.2+). It hard-requires `ext-ctype`/`ext-iconv`/`ext-xml`; `ctype` is ported, and a minimal
  attribute-routed, prod-cached page never calls `iconv`/XML at runtime, so those two are bypassed with
  composer platform overrides. A minimal, XML-free slice — anything using DOM/XML would need a
  `libxml2` port.
- **FATFS/POSIX fixes to run larger frameworks** (`main/fs_pathnorm.c`, new `main/fs_glob.c`),
  surfaced by Symfony's bootstrap and shared by every web-server app: `stat()`/`access()` on a
  mount-point root (`/sdcard`) now report the directory as existing; a `readdir`/`fnmatch`-based
  `glob()` replaces picolibc's broken one (returning empty-success, not `GLOB_NOMATCH`, so PHP's
  `glob()` yields `[]`); `rename()` now overwrites an existing destination (unlink-and-retry) for
  atomic file replaces; and the `web-server` model presents a `cli-server` SAPI name so frameworks
  take the HTTP path instead of a `php://stdout` console path. See
  [`docs/reference/porting-notes.md`](docs/reference/porting-notes.md).
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
  [`https-client`](examples/https-client/) example and `docs/extensions/openssl.md`.
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
    the flags and run the fetch. `docs/extensions/openssl.md` explains when to use which.
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
- `docs/extensions/porting-status.md`: the status of every PHP 8.3 bundled extension — built-in, behind a
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
  ctype ~2.5 KB, filter ~27 KB, mbstring ~965 KB (see `docs/reference/footprint.md`).
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
- `docs/reference/footprint.md`: flash and RAM usage, per area and per optional extension.

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
- Documentation: README, plus `docs/getting-started/architecture.md`, `docs/reference/porting-notes.md`,
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
