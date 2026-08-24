# Porting notes

The technical choices behind running PHP 8.3 on the ESP32, with the reason for each one. This is the
place to look when something seems arbitrary: it almost never is. The work started on the ESP32-P4
(RISC-V) and now covers the ESP32-S3 (Xtensa) as well; most of it is target-independent by design, and
the parts that are not are called out where they come up (the per-board storage and networking near
the end, and the couple of arch notes below).

## The hand-written config

I don't use `./configure`. In its place there's a hand-written `php_config.h`, started
from the `php_config.h` that `configure` generates on Linux and then corrected.

The most important and least obvious correction is about type sizes. The reference file
came from an x86-64, where `long`, `size_t` and pointers are 8 bytes; both supported targets
are 32-bit (RISC-V on the P4, Xtensa on the S3), where they are 4. Wrong `SIZEOF_LONG`,
`SIZEOF_SIZE_T`, `SIZEOF_OFF_T`, `SIZEOF_PTRDIFF_T` and `SIZEOF_SSIZE_T` caused subtle damage
until I lined them up with the real values read from the compiler. The values are the same for
both chips, since both are 32-bit ILP32 with newlib, so this part is not architecture-specific.

Then I turned off the dozens of `HAVE_*` that are true on glibc and false on newlib:
dynamic loading (`dl`), `mmap`, `poll`, `fopencookie`, `statvfs`, the process and network
headers, POSIX timers and signals. Where the code has a fallback branch it falls into it on
its own; where it doesn't, that piece is left out.

A few defines, on the other hand, I add from the component's `CMakeLists.txt`:
`HAVE_INT32_T`/`HAVE_UINT32_T` (to stop timelib's duplicate typedefs), and the constants
lwip is missing (`NI_*`, `AF_UNIX`, `PF_UNIX`).

A couple of headers that `configure` would generate I provide by hand, because the source
looks for them by name: `zend_config.h` and `main/php_config.h` are tiny shims that point back
to our `php_config.h`, `build-defs.h` holds cosmetic values (paths for `phpinfo`, all pointing
at `/sdcard`), and `internal_functions.c` lists the static modules to load at startup.

## One directory per PHP version

Everything that is specific to a PHP version lives under `components/php/versions/<version>/`,
so supporting a new PHP release is a new directory rather than edits scattered across the build.
Each version directory owns: the hand-written headers above (`php_config.h`, `zend_config.h`,
`build-defs.h`, `main/php_config.h`, `internal_functions.c`); the source list and include dirs
(`sources.cmake`); the optional-extension wiring (`extensions.cmake`); the patches
(`patches/php/`); the extension manifest flash-tool reads (`manifest.toml`); and the tarball
coordinates (`version.env`: version + sha256). The version-sensitive parts of `compat/` live
here too — `date_stub.c`, `timelib_config.h`, `timezonedb_minimal.h` track timelib, which
changes between PHP versions — and the version include dir comes first on the path, so a version
can override any shared `compat/` file without duplicating the ones it doesn't touch.

`components/php/CMakeLists.txt` is generic: it picks `PHP_VERSION` (default from `php-esp32.toml`
at the repo root; override with `-DPHP_VERSION=<ver>`), then includes that version's
`sources.cmake` and `extensions.cmake`. `scripts/fetch-php.sh` reads the same version's
`version.env` and applies its patches. Truly version-agnostic pieces stay shared at the
component root: the platform `compat/` (`posix_stubs.c`, `syslog.*`, `sqlite-compat.h`, `sys/`)
and the external dependencies (the SQLite amalgamation and oniguruma, which track their own
upstreams, not PHP's).

## One directory per board (and chip family)

The same idea applies to the hardware. Everything specific to a board lives under
`boards/<family>/<board>/` — currently `boards/esp32-p4/esp32-p4-pico/` and
`boards/esp32-p4/esp32-p4-eth/`. A board owns three things: its **config**
(`sdkconfig.board`: flash size, chip revision, console, partition table), its **pins**, and
its **code** — `board.c` implements a small interface (`board.h`: `board_mount_storage()` /
`board_unmount_storage()`), so the microSD wiring (4-bit SDMMC on GPIO39-44, powered by the
on-chip LDO on channel 4) lives with the board, not in `main.c`. A different board — even one
that wires storage completely differently (SPI SD, other pins, no LDO) — is a new directory
implementing the same functions; `main/main.c` is board-agnostic and just calls them. A board
also carries a `board.toml` declaring which storage types (`microsd`/`embedded`) and execution
modes (`init-loop`/…) its hardware supports — read by `flash-tool`, intersected with what the
firmware implements (the version manifest). That's where the two P4 boards differ: the Pico has
no wired network so it omits `web-server`, while the `esp32-p4-eth` (an on-board IP101GRI
Ethernet PHY) advertises it — offered once the firmware implements it. The two share the same P4
SD reference design and pin-out; the ETH adds only a GPIO45 high-side switch on the card's VDD,
which its `board.c` enables before mounting.

Chip-family settings sit one level up, in `boards/<family>/sdkconfig.family` (the ESP-IDF
target, PSRAM), shared by every board of that family; a future `esp32-s3/` family is a sibling
directory. `sdkconfig` is layered base → family → board (most specific wins): the repo-root
`sdkconfig.defaults` holds only board-agnostic PHP-runtime knobs (the bumped task stack, FAT
long filenames). The top-level `CMakeLists.txt` resolves the board (default `default_board` in
`php-esp32.toml`, override `-DBOARD=<board>`), assembles that three-file `SDKCONFIG_DEFAULTS`
list, and the `main` component pulls in the board's `board.cmake` (its sources, include dir and
driver requirements). Changing board or family regenerates `sdkconfig` (`rm sdkconfig` first).

## Where the PHP source lives: microSD or embedded

`index.php` (and its `vendor/`) can come from one of two places, and the firmware supports both
at once. The default is the **microSD**: the card is mounted read-write at `/sdcard` and the app
runs `/sdcard/index.php`. Editing the code is just swapping the file on the card — no rebuild.

**Embedded** puts the source inside the chip instead, for a board that ships without a card. The
partition table gains a `storage` partition (type `fat`), sized to the source by
`cmake/gen-partitions.cmake` — a `microsd` project has none at all (see
[footprint.md](footprint.md)). The top-level `CMakeLists.txt` builds a **read-only FAT image** from a
source directory when you pass `-DPHP_EMBED_SRC=<dir>`:

```cmake
fatfs_create_rawflash_image(storage "${PHP_EMBED_SRC}" FLASH_IN_PROJECT)
```

That runs ESP-IDF's `fatfsgen.py` with long-filename support (Composer's `vendor/` has paths well
past 8.3, so LFN is required, same as the SD) and produces `build/storage.bin`, which `idf.py
flash` writes to the `storage` offset alongside the app. FAT, not SPIFFS: SPIFFS caps filenames at
32 characters, which breaks a real `vendor/` tree. Without `PHP_EMBED_SRC` the partition is simply
left empty and nothing changes — the same firmware still runs from the card.

At boot `main.c` mounts both sources: the microSD at `/sdcard` (when a card is present) and, if the
`storage` partition holds an image, that read-only image at `/app` (via
`esp_vfs_fat_spiflash_mount_ro`). It runs `/app/index.php` when present, otherwise `/sdcard/index.php`.
The two are not exclusive: an embedded project that needs to *write* (a SQLite database, logs) still
gets a writable microSD mounted at `/sdcard`, because the embedded image itself is read-only. So
"embedded" is really "source in flash, data still on the card if you want it".

microSD support is itself optional (`-DPHP_STORAGE_MICROSD`, on by default). A `microsd` project
needs it (the source is on the card); an `embedded` one defaults to **off** — a self-contained
firmware that never touches a card. Off, `main.c` doesn't compile the mount call and the board's
`board.cmake` doesn't pull the SDMMC drivers (the whole SD block in `board.c` is behind the same
`#ifdef`), so it's ~51 KB smaller and a board with no card slot builds cleanly. `fatfs`/`vfs` stay,
since the embedded image is FAT too. An embedded project opts the card back in with
`[storage] microsd = true`. Like the board and web-server flags, the flag isn't visible in the
early requirement-expansion phase, so the top-level exports it to the environment for `board.cmake`.

## Networking and the web-server model

A board with wired Ethernet (the `esp32-p4-eth`) declares `BOARD_HAS_NETWORK`, and its `board.c`
implements `board_network_up()`: it initialises the EMAC and the IP101 RMII PHY, starts a DHCP
client, waits (bounded) for a lease and returns the address. The ESP32-P4's default EMAC pin map
already matches this board's wiring, so the MAC config is the stock `ETH_ESP32_EMAC_DEFAULT_CONFIG()`
plus the PHY reset pin. `main.c` calls this for any board that has it and logs
`network up -- http://<ip>/`; a board without a network never links the code in.

On top of that, PHP can serve HTTP two ways. The plain way is entirely in PHP: a script opens
`stream_socket_server('tcp://0.0.0.0:80')` and accepts connections in `loop()` — the socket calls
go through lwIP, no extension needed (`ext/sockets` isn't built; the stream server is core). The
other way is the **`web-server` project type** (`-DPHP_PROJECT_WEB_SERVER=ON`): a C HTTP server
(`esp_http_server`) runs in front and PHP is invoked fresh per request, like a script behind
Apache. The subtlety there is running one PHP *request* per HTTP request: `php_embed_init()` opens
a single request, so the firmware closes it and then cycles `php_request_startup()` → run the
script → `php_request_shutdown()` for each hit (shared-nothing, so re-running the top-level script
doesn't hit "cannot redeclare"). Output is captured by pointing the **live** `sapi_module.ub_write`
(not `php_embed_module`'s — `sapi_startup` already copied that) at a buffer, and `$_SERVER` is
filled by forcing that JIT auto-global to materialise and then `php_register_variable()`. PHP runs
in the main task, which has the big stack the compiler needs; the httpd task just parks the request
and waits, so it can keep a small stack.

One more subtlety for real frameworks: **the SAPI name.** `php_embed` calls its SAPI `embed`, and
frameworks branch on `php_sapi_name()`. Symfony's front controller treats an unknown/CLI-like SAPI as
a console context and reaches for `php://stdout`, which doesn't exist here — the request crashed
before rendering. Under the `web-server` model the firmware therefore renames the SAPI to
**`cli-server`** (`php_embed_module.name = "cli-server"`, set before `php_embed_init()`), the name
PHP's own built-in web server uses. Frameworks recognise it as a web SAPI and take the HTTP path. This
applies to every `web-server` app, not just Symfony.

One fix this needed: **the CSPRNG.** `ext/random`'s `csprng.c` only reaches for `getrandom()` on
Linux/BSD and otherwise opens `/dev/urandom`, which doesn't exist here — so `random_int()`,
`random_bytes()` and anything built on them threw `Random\RandomException`. ESP-IDF's newlib
*does* provide a working `getrandom()` (backed by `esp_fill_random`, the hardware RNG), so a patch
(`patches/php/0004-csprng-esp-getrandom.patch`) enables the `getrandom()` path whenever the libc
has it. PHP's random functions then work — which real web apps (session ids, CSRF tokens) lean on.

(Aside on the build: `-DBOARD` and `-DPHP_PROJECT_WEB_SERVER` are invisible in ESP-IDF's early
requirement-expansion phase, where a component's `REQUIRES` are gathered. A non-default board used
to silently inherit the *default* board's component requirements there; it only bit once the ETH
board needed networking components the Pico doesn't. The top-level `CMakeLists` now exports both
into the environment, which the early-phase code reads as a fallback.)

## The virtual machine

PHP has several virtual-machine variants. The fast one, "hybrid", uses computed-gotos and
GCC's global registers, and on RISC-V it simply doesn't compile (a variable ends up out of
scope). Turning off `HAVE_GCC_GLOBAL_REGS` switches to the "call" variant, which is plain C
code: a loop that calls each opcode's function. It's a touch slower, but it's portable and
it's the standard choice for embedded environments. This is what makes the port
architecture-independent: the call VM has no target-specific assembly, so the same engine
compiles unchanged on the S3's Xtensa core as on the P4's RISC-V. Bringing up the S3 needed no
change to any PHP source, only its own board and toolchain.

## The disabled features

Some engine features rely on services that don't exist here, and I keep them off. POSIX
signals (`ZEND_SIGNALS`) aren't there under FreeRTOS. Execution timers (`setitimer`,
`ZEND_MAX_EXECUTION_TIMERS`) aren't either, and the timeout is zero anyway. PCRE2's JIT is
off because it would need writable executable memory, which has no sane story here. And the
x87 assembly for floating-point precision control is obviously worthless on RISC-V.

## The POSIX stubs

PHP calls a pile of functions that newlib doesn't implement on this target. They all live
in one place, `components/php/compat/posix_stubs.c`, as *weak* stubs: if ESP-IDF ever
provides the real function, that one wins; in the meantime the stub keeps the link from
coming up short, and fails cleanly instead of corrupting something. The categories:

- Processes and exec (`popen`, `pclose`, `posix_spawn*`, `waitpid`, `pipe`, `socketpair`,
  `nice`): there's no way to spawn processes, so they return an error.
- Users and groups (`getuid`, `getgid`, `getpwnam`, `getgrnam`...): there's no concept of a
  user, so zero or `NULL`.
- POSIX filesystem (`umask`, `dup`, `chown`, `symlink`, `readlink`, `lstat`, `flock`,
  `glob`...): whatever FATFS doesn't cover.
- Network (`getnameinfo`, `gai_strerror`, `gethostname`): no resolver.
- Memory (`mmap`, `munmap`, `mprotect`): no memory mapping.
- Time (`nanosleep` mapped onto `usleep`, `signal` a no-op).

Two cases deserve a note. The first is `syslog`, provided here by a small `compat/syslog.h`
and ending up on stderr (that is, on the console). The second is `Fiber`: it would need
boost.context's context-switch assembly, which only exists for 64-bit RISC-V, not 32-bit.
The two symbols (`make_fcontext`, `jump_fcontext`) are filled with inert stubs: PHP's
`Fiber` class would crash if used, but the setup/loop model doesn't need it, and the rest
of the language works.

## The ext/date stub (and the real thing, optionally)

`ext/date` is off by default. A few core files call some of its functions, so
`components/php/compat/date_stub.c` has the bare minimum: `php_time()` returns `time(NULL)`,
`php_format_date()` does a minimal `strftime`-based formatting in UTC (enough for log
timestamps, it doesn't interpret PHP's format characters), and the timezone functions answer
as "absent/UTC".

The real `ext/date` — the `DateTime` class and the full date/time API — builds in as an
optional extension (`-DPHP_EXT_DATE=ON`, or `flash.sh` asks). When it's on, timelib provides
`php_time()`/`php_format_date()` and the stub is dropped (it would be a duplicate symbol). It
costs about **650 KB** of flash, of which ~350 KB is the builtin timezone database. The only
code change it needed was turning off `HAVE_STRUCT_TM_TM_GMTOFF`/`HAVE_STRUCT_TM_TM_ZONE` in
`php_config.h`: newlib's `struct tm` doesn't have those fields.

A sub-option, `-DPHP_EXT_DATE_MINIMAL_TZ=ON`, swaps the full builtin timezone database for a
UTC-only one (`compat/timezonedb_minimal.h`, ~2.7 KB), saving ~350 KB. `DateTime` then works
in UTC only; named zones (e.g. `Europe/Rome`) report an error. It's carried as a one-line
`parse_tz.c` patch (`components/php/patches/php/`, applied by `fetch-php.sh`).

## Compiler warnings

The PHP source is old C full of idioms that GCC 14 promotes to errors (incompatible
pointers, implicit declarations). I compile it with `-w -fpermissive`, which brings those
back to warnings. My own code (in `main/` and `compat/`), instead, stays under ESP-IDF's
full warnings, so any mistakes of mine surface.

## Output and console

The embed SAPI's output funnel, by default, believes the connection has dropped and calls
`exit()` if it writes fewer bytes than requested, and `exit()` on ESP-IDF goes straight to
`abort()`. I put in a custom `ub_write` (in `main.c`) that writes to the console and always
returns the full length: no false alarms.

There's also a buffering issue: keeping stdout unbuffered pushes newlib into a print path
that creates a lock on every call, and on this target creating that mutex would abort. It's
enough to put stdout and stderr in line-buffering.

Finally, the calls to `setup()`/`loop()` are wrapped in `zend_try`/`zend_catch`: a fatal
error in the script is caught instead of taking the board down.

## Memory: PHP in PSRAM

This is the knot that took the most work to understand. With the default configuration,
ESP-IDF sends every allocation below a certain threshold (16 KB) to internal RAM. PHP makes
thousands of small allocations, so `php_embed_init()` ate all the internal RAM: from 388 KB
free down to 19 bytes. That starved the DMA-capable memory, and the first read from the
microSD failed because the SDMMC driver couldn't allocate its buffer.

The fix is to set that threshold to zero (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0`): then all
of PHP's `malloc`s go to PSRAM, and internal RAM stays free (about 429 KB) for DMA and
FreeRTOS objects. The allocations that *must* be internal still go there, because they ask
for it with an explicit flag. After the fix, internal RAM is unchanged before and after the
engine starts.

## Bring-up on silicon

Some things only show up with the chip in hand.

The chip revision. The board carries an ESP32-P4 revision v1.3, and ESP-IDF 5.5 by default
refuses to flash pre-3.0 revisions (the production silicon is different). It's unlocked with
two lines in `sdkconfig.defaults`: `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` and
`CONFIG_ESP32P4_REV_MIN_100=y`.

Powering the microSD. The card didn't respond at all (every command timed out) until I
found out it's powered by an internal LDO of the chip, channel 4, which has to be turned on
in software with `sd_pwr_ctrl_new_on_chip_ldo`. A detail confirmed by Waveshare's official
SD example, which enables it by default. The microSD is wired as 4-bit SDMMC: CLK on GPIO43,
CMD on GPIO44, D0-D3 on GPIO39-42.

The console. The board's USB-C port is not the ESP32's native USB: it's a USB-UART bridge
(CH343P) wired to the chip's UART0. Convenient, because a single USB-C cable does flashing
and console with automatic reset. But it means the P4's native USB is elsewhere, on a
separate connector: that's why the "USB stick" variant was left out.

## The GPIO extension

`components/php_ext_gpio/` is a small PHP extension that exposes `gpio_mode`, `gpio_write`,
`gpio_read` and `delay`, plus the `GPIO_INPUT`/`GPIO_OUTPUT` constants. `delay()` maps onto
`vTaskDelay`, never a busy-wait: yielding the core keeps the watchdog happy. The module is
registered among the static extensions in `internal_functions.c`, and the component is
marked `WHOLE_ARCHIVE` because the reference to the module comes only from a data table, and
without that flag the linker wouldn't pull it into the archive.

## Running Composer and frameworks

Loading a Composer `vendor/` tree from the microSD took two things.

FAT long filenames. By default FATFS on ESP-IDF only knows 8.3 names, and a file like
`ShouldHandleEventsAfterCommit.php` won't even open: `f_open` returns `FR_INVALID_NAME`,
which becomes an `EINVAL` ("Invalid argument") on the PHP side. Enabling long names in
`sdkconfig.defaults` is enough (`CONFIG_FATFS_LFN_HEAP`, a name buffer on the heap, which
here lands in PSRAM).

The closure run-time cache. With long names in place autoloading started, but frameworks
use a great many scope-bound closures, and there a heap corruption showed up. The diagnosis
came from `HEAP_POISONING_COMPREHENSIVE`, which stops the board at the exact point of the
corruption: the culprit was the per-closure `efree()` of the run-time cache in
`destroy_op_array()`. The same PHP without opcache runs fine on a PC, so the problem is
specific to this environment (no opcache, `USE_ZEND_ALLOC=0`). The patch — in
`components/php/patches/`, applied by `fetch-php.sh` — allocates that cache from the request
arena instead of with `emalloc`, so it's freed all at once at the end of the run and never
one closure at a time. The price is that the cache isn't reclaimed per closure: a script
that creates them by the bucketload inside a forever `loop()` grows in memory until the run
ends. For one-shot scripts it makes no difference.

## SQLite on the SD card (FATFS is not POSIX)

Getting the optional PDO/SQLite extension to write a real database on the microSD ran into a
subtle FATFS-vs-POSIX difference that took a while to pin down. Plain file writes worked fine
(`file_put_contents` writes, reads back, persists), yet SQLite always ended up with a corrupt
file and `SQLITE_NOTADB` — "file is not a database".

The difference is *how* the two write. `file_put_contents` writes sequentially from offset 0;
SQLite `lseek()`s to an offset and then reads or writes there. And on ESP-IDF's FATFS,
**`lseek()` past end-of-file expands the file**, filling the new area with raw `0xFF` — unlike
POSIX, where seeking past EOF is a no-op until you actually write. SQLite seeks past EOF to
read header fields of a fresh database; that bare `lseek` grew the file to a few bytes of
`0xFF`, so SQLite then read a bogus header and declared the file "not a database". Instrumenting
the VFS made it plain: on a freshly created 0-byte db, a lone `lseek` to offset 24 made
`fstat` jump from 0 to 24 bytes, with no write in between.

The fix is a patch to the amalgamation (in `components/php/patches/sqlite/`, applied by
`fetch-sqlite.sh`), in two places:

- **Reads** (`seekAndRead`): a read at or beyond the current size returns end-of-file (0)
  instead of doing the expanding `lseek`.
- **Writes** (`seekAndWriteFd`): a write past EOF would otherwise leave a `0xFF` hole (POSIX
  would zero-fill it). Here `ftruncate` *does* zero-fill, so the file is grown to the write
  offset with it first, turning the hole into zeros.

The lesson for this board: FATFS gives you `open`/`read`/`write`/`lseek`, but not their POSIX
semantics for sparse files — anything that seeks past EOF has to be guarded.

## Three more bundled extensions (ctype, mbstring, filter)

Carrying the port a bit further with three more of PHP's bundled extensions: `ext/ctype`
(character-class checks), `ext/mbstring` (multibyte strings) and `ext/filter` (`filter_var()`
validation and sanitization). All three are optional, off by default, each with its own
`-DPHP_EXT_<NAME>=ON` flag. Two of them needed a porting touch:

- **ctype** wraps its whole body — the module entry included — in `#ifdef HAVE_CTYPE`, a macro
  `configure` would define. The hand-written `php_config.h` leaves it undefined, so without help
  the file compiles to nothing and the link fails with `undefined reference to ctype_module_entry`.
  The fix is to define `HAVE_CTYPE` for that one file only (scoped in CMake), not globally.
- **mbstring** is built **without** oniguruma by default (a separate regex library PHP doesn't
  bundle), so the `mb_ereg*` and `mb_split` functions are left out (`php_mbregex.c` isn't
  compiled and `HAVE_MBREGEX` stays undefined); everything else — length, case, `substr`,
  encoding detect/convert — is there. Code that needs `mb_split` can polyfill it over PCRE in a
  couple of lines (the `eloquent-demo` example does exactly this), **or** turn on the real thing
  (next bullet). Its one build quirk is that `mbstring.c`
  does `#include "libmbfl/config.h"`, a header `configure` generates; we generate a one-line shim
  in the build tree (it just pulls in `php_config.h`) and put it on the include path. The bundled
  libmbfl is the reason mbstring is by far the heaviest extension (~965 KB — see
  [footprint.md](footprint.md)): most of that is the CJK conversion tables.
- **mbstring without CJK.** Because those tables (`mbfilter_cjk.c`) are ~740 KB of the total,
  a sub-option `-DPHP_EXT_MBSTRING_NO_CJK=ON` drops them, taking mbstring down to ~209 KB. It
  leaves out `mbfilter_cjk.c` and `mbfilter_utf8_mobile.c` (which reuses the CJK emoji tables),
  and a patch (`patches/php/0003-...`, gated by the `MBSTRING_NO_CJK` macro so it's a no-op
  otherwise) removes the three things that would otherwise reference the missing symbols: the
  CJK/mobile entries in the encoding registry (`mbfl_encoding.c`), a `sjis_mac` fast-path check
  in `mb_substr`, and the whole `mb_convert_kana()` function (Japanese kana, whose lookup lives
  in the CJK file — including its entry in the generated `mbstring_arginfo.h` function table).
  UTF-8/UTF-16/Latin and everything else are unaffected; the named CJK codecs (Shift-JIS,
  EUC-*, Big5, …) simply aren't registered.
- **mbstring with mb_ereg (oniguruma).** To get the `mb_ereg*`/`mb_split` regex family for real,
  `-DPHP_EXT_MBSTRING_ONIG=ON` compiles `php_mbregex.c` and defines `HAVE_MBREGEX` (which is what
  makes `mbstring.c` and its arginfo register those functions). Since PHP doesn't bundle the
  regex engine, this **vendors oniguruma** (`scripts/fetch-oniguruma.sh`, git-ignored, like the
  SQLite amalgamation) and compiles its ~48 library sources. Two port touches: oniguruma expects
  a `configure`-generated `src/config.h`, so the fetch script drops in a hand-written one (a few
  `HAVE_*_H` and the `SIZEOF_*` for ILP32); and `php_mbregex.c` is compiled with
  `ONIG_ESCAPE_UCHAR_COLLISION=1` (so `oniguruma.h` doesn't re-`typedef UChar`, which `php.h`
  already has) and the `oniguruma/src` include — both scoped to that one file. It adds ~445 KB
  (see [footprint.md](footprint.md)); off by default, since most code doesn't need mb-regex.
- **filter** needed nothing special: it defines its module entry unconditionally and leans only
  on `ext/pcre` and `ext/standard`, both always in.

## The openssl extension, in two flavours (and a TLS client)

`ext/openssl` is written against the OpenSSL C library, which doesn't exist for this target
(ESP-IDF ships mbedTLS, a different API). So it comes in two forms a project picks between —
`[extensions.openssl] full = true|false`. The reference doc is [openssl.md](openssl.md); the port
touches worth recording:

- **The compatible subset** (`-DPHP_EXT_OPENSSL=ON`, default, ~42 KB) is a *hand-written* extension
  (`components/php/compat/openssl_compat.c`) backed by ESP-IDF's mbedTLS. It provides just the
  symmetric-cipher surface — `openssl_encrypt`/`decrypt` (AES-128/192/256 CBC+GCM),
  `openssl_cipher_iv_length`, `openssl_random_pseudo_bytes` (hardware RNG), the `OPENSSL_RAW_DATA`/
  `OPENSSL_ZERO_PADDING` constants — enough for e.g. a framework's encrypter. It registers the same
  `openssl` module name and is byte-for-byte interoperable with desktop OpenSSL (KAT-verified). No
  RSA/X.509/TLS.

- **The full build** (`-DPHP_EXT_OPENSSL_FULL=ON`, ~2.1 MB) is the *real* `ext/openssl/openssl.c`
  compiled against a **ported OpenSSL 3.0 libcrypto**. `scripts/fetch-openssl.sh` cross-compiles
  OpenSSL for `riscv32-esp-elf` + newlib, **static and `no-pic`** — bare metal has no dynamic loader,
  so a `.got.plt` would fail the final link (`ld: discarded output section .got.plt`). It's
  configured `no-sock no-dgram no-threads no-engine no-legacy …`, seeded `--with-rand-seed=getrandom`
  (newlib's `getentropy`→`getrandom`→`esp_random`), with a one-line `<syslog.h>` shim newlib lacks.
  Two compile touches for `openssl.c` itself: `-Dtimezone=_timezone` (newlib spells the POSIX global
  `_timezone`), and a stubbed `OPENSSL_init_ssl` (we don't link libssl). A `RAND_METHOD` backed by
  `esp_fill_random` is installed at startup so the legacy RNG path also uses the hardware RNG.

- **On-chip key generation needed a config file.** `openssl_pkey_new()` first failed with
  *"configuration file routines: no such file"* — OpenSSL 3.0 brings its providers up by reading an
  `openssl.cnf`, and there's no filesystem default on the chip. Setting `OPENSSL_INIT_NO_LOAD_CONFIG`
  at runtime did **not** fix it; shipping a minimal `openssl.cnf` (that just activates the default
  provider) and pointing `OPENSSL_CONF` at it — the firmware does this in `main.c`, path from
  `-DPHP_OPENSSL_CONF` — did. RSA-2048 keygen then works (verified on hardware); it's CPU-bound
  (~20-45 s, so the task-watchdog window is widened to 60 s). The `no_load_config` setting flips back
  to the config-less init for devices that only *use* provisioned keys. EC keygen still needs curve
  defaults the minimal cnf doesn't carry.

- **The TLS client rides mbedTLS, not libssl.** `openssl.c`'s MINIT registers the `ssl://`/`tls://`
  stream transports, but pointing them at OpenSSL's own `xp_ssl.c` would need **libssl + BSD
  sockets** — and the standalone OpenSSL cross-build has neither (the sysroot has no lwIP sockets,
  hence `no-sock`). So with the `tls` setting (`-DPHP_EXT_OPENSSL_TLS=ON`) the transport factory
  (`php_openssl_ssl_socket_factory`, stubbed to "refuse to open" otherwise) is implemented in
  `components/php/compat/openssl_tls_esptls.c` over ESP-IDF's **esp-tls/mbedTLS**, which is compiled
  *inside* ESP-IDF where lwIP and mbedTLS are available. `esp_tls_conn_new_sync()` does DNS + TCP +
  handshake in one call; the factory wraps it as a `php_stream` (a transport whose `set_option`
  handles the `XPORT_API` connect op), so PHP's normal stream layer reaches HTTPS —
  `file_get_contents('https://…')`, `stream_socket_client('tls://…')`. The crypto stays real OpenSSL
  (libcrypto); only the TLS record layer is mbedTLS. Client only, and a networked board is required.

- **Software AES was mandatory for TLS.** The first handshake died with *"esp-aes: Failed to
  allocate memory for the array of DMA descriptors"*: the PHP heap is PSRAM (`USE_ZEND_ALLOC=0` →
  `malloc` → PSRAM), so the TLS record buffers live in PSRAM, and the hardware AES accelerator drives
  DMA that can't reach PSRAM. `sdkconfig.defaults` disables it (`CONFIG_MBEDTLS_HARDWARE_AES=n`,
  `…_GCM=n`); software AES works from any memory. A handshake then takes ~10 s (software RSA verify +
  AES), which the watchdog window covers.

- **Certificates and DNS.** TLS verification uses a CA bundle the firmware reads from
  `$PHP_TLS_CAFILE` (path from `-DPHP_TLS_CAFILE`, default `certs/ca-bundle.crt` under the source);
  `phpflash` copies the host's root store into the project (and `phpflash update-certs` refreshes
  it). With no bundle the client connects but doesn't verify (and logs it). Static DNS servers
  (`[network] dns`) are applied to the netif after DHCP via `-DPHP_NET_DNS` (comma-separated — a
  semicolon would be split by CMake's list syntax). See the `https-client` example.

## FATFS path resolution: `.`/`..` and `lstat` (running frameworks)

Real frameworks lean on POSIX filesystem semantics the ESP-IDF FATFS VFS doesn't fully provide. Two
gaps surfaced running unmodified Laravel (`examples/laravel-demo/`), fixed once and for all in
`main/fs_pathnorm.c` — general fixes, not Laravel-specific.

- **`.` and `..` aren't resolved.** FatFs takes the path verbatim, so `stat("/a/b/../c")` fails even
  when `/a/c` exists. Frameworks build such paths constantly (Laravel's config loader uses
  `__DIR__."/../../../../config"`). `fs_pathnorm.c` provides linker `--wrap` shims around the file
  syscalls (`stat`, `lstat`, `open`, `opendir`, `access`, `mkdir`, `unlink`, `rmdir`, `rename`) that
  collapse `.`/`..`/`//` lexically before the VFS sees the path. Clean paths pass straight through.
  The `-Wl,--wrap=…` options are added globally (`idf_build_set_property(LINK_OPTIONS …)`), so every
  caller — PHP included — is covered.
- **`lstat` is unimplemented.** The FATFS VFS has `stat` but its `lstat` fails, and PHP's own
  `realpath()` (`tsrm_realpath`) `lstat`s every path component — so `realpath()` returned `false` for
  *everything*, and Symfony's `SplFileInfo::getRealPath()` (→ `realpath()`) went empty, ending in
  `require("")` → `ValueError`. FAT has no symlinks, so `lstat` ≡ `stat`; the `lstat` wrap routes to
  `stat`, and `realpath()` works.

A dead end worth recording: PHP's own virtual-cwd (`VIRTUAL_DIR`) *would* normalize `.`/`..`, but it
depends on a working `getcwd()`/cwd this target lacks — enabling it made `stat()` fail for every
path (even the running script). Don't enable `VIRTUAL_DIR`; normalize at the syscall layer instead.

Three more FATFS/POSIX gaps surfaced running unmodified **Symfony 7.4** (`examples/symfony-demo/`) —
its bootstrap exercises more of the filesystem than Laravel did. All fixed generically:

- **`stat()` on a mount-point root fails.** `stat("/sdcard")` returns an error even though files
  *inside* it stat fine — the VFS resolves the mount and hands FatFs an empty path. Symfony's
  `FileLocator` does `file_exists("/sdcard")` on the document root and concluded it didn't exist.
  The `stat`/`lstat`/`access` wraps in `fs_pathnorm.c` now treat a failing stat of a known mount root
  (`/sdcard`, `/app`) as an existing directory (`is_mount_root()` → a synthetic `S_IFDIR` stat).
- **`glob()` is missing/broken.** picolibc's `glob()` doesn't work on this VFS, and PHP leans on it
  (Symfony's `GlobResource`, config globbing). `main/fs_glob.c` provides `__wrap_glob`/`__wrap_globfree`
  built on `readdir` + `fnmatch` (handling `*?[` per component and one level of `{a,b}` braces). The
  wrap is force-linked (`-Wl,-u,__wrap_glob`) since `glob` has too few callers to pull in otherwise.
  One subtlety: on *no match* it returns empty-success (`gl_pathc = 0`), **not** `GLOB_NOMATCH` —
  PHP's `dir.c` only treats `GLOB_NOMATCH` as "empty, not an error" behind an `#ifdef` that picolibc's
  headers compile out, so returning the code would make PHP's `glob()` return `false` instead of `[]`.
- **`rename()` won't overwrite an existing destination.** POSIX `rename()` atomically replaces the
  target; FatFs's fails if the target exists. Symfony writes its compiled container to a temp file and
  renames it over the previous one (`Cannot rename … : File exists`). The `rename` wrap now, on
  failure, `stat`s the destination and — if it exists — `unlink`s it and retries, restoring the
  atomic-replace behaviour callers expect.
