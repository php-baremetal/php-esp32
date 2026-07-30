# Porting notes

The technical choices made to port PHP 8.3 to the ESP32-P4, with the reason behind each
one. This is the place to look when something seems arbitrary: it almost never is.

## The hand-written config

I don't use `./configure`. In its place there's a hand-written `php_config.h`, started
from the `php_config.h` that `configure` generates on Linux and then corrected.

The most important and least obvious correction is about type sizes. The reference file
came from an x86-64, where `long`, `size_t` and pointers are 8 bytes; the ESP32-P4 is
32-bit RISC-V, where they're 4. Wrong `SIZEOF_LONG`, `SIZEOF_SIZE_T`, `SIZEOF_OFF_T`,
`SIZEOF_PTRDIFF_T` and `SIZEOF_SSIZE_T` caused subtle damage until I lined them up with the
real values read from the compiler.

Then I turned off the dozens of `HAVE_*` that are true on glibc and false on newlib:
dynamic loading (`dl`), `mmap`, `poll`, `fopencookie`, `statvfs`, the process and network
headers, POSIX timers and signals. Where the code has a fallback branch it falls into it on
its own; where it doesn't, that piece is left out.

A few defines, on the other hand, I add from the component's `CMakeLists.txt`:
`HAVE_INT32_T`/`HAVE_UINT32_T` (to stop timelib's duplicate typedefs), and the constants
lwip is missing (`NI_*`, `AF_UNIX`, `PF_UNIX`).

A couple of headers that `configure` would generate I provide by hand in the component
root, because the source looks for them by name: `zend_config.h` and `main/php_config.h`
are tiny shims that point back to our `php_config.h`, `build-defs.h` holds cosmetic values
(paths for `phpinfo`, all pointing at `/sdcard`), and `internal_functions.c` lists the
static modules to load at startup.

## The virtual machine

PHP has several virtual-machine variants. The fast one, "hybrid", uses computed-gotos and
GCC's global registers, and on RISC-V it simply doesn't compile (a variable ends up out of
scope). Turning off `HAVE_GCC_GLOBAL_REGS` switches to the "call" variant, which is plain C
code: a loop that calls each opcode's function. It's a touch slower, but it's portable and
it's the standard choice for embedded environments.

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
