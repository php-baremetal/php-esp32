---
eyebrow: 'Docs · Extensions'
lede: 'Zend OPcache ported to the ESP32 without JIT and statically linked. It caches the compiled bytecode so a request stops recompiling the framework every time, in one of two modes: a file cache on the microSD, or an in-RAM cache held in PSRAM.'
see_also:
  - href: ./porting-status.md
    meta: Extensions
    label: Porting status
  - href: ../reference/footprint.md
    meta: Reference
    label: Footprint
  - href: ../storage/persistent-store.md
    meta: Storage
    label: Persistent store
prev:
  label: OpenSSL
  href: ./openssl.md
next:
  label: Custom C extensions
  href: ./custom-extensions.md
---

# OPcache

Without an opcode cache, PHP recompiles every script on every request: tokenize, parse, compile,
optimize. For one small script that is cheap. For a framework that pulls in hundreds of files it
dominates the request time. Zend OPcache caches the compiled bytecode so the next run skips all of
that.

This is a real port of the bundled `ext/opcache`, not a stub, verified on ESP32-P4 hardware. It is
built without JIT (unsupported on these targets) and statically linked into the firmware, running in
one of two cache modes.

## Two cache modes

The port keeps the bytecode either on the microSD or in PSRAM. The two modes differ only in where the
compiled bytecode lives and what that costs, not in what OPcache does with it.

| | File cache (default) | In-RAM SHM (`in_memory`) |
|---|---|---|
| Bytecode lives | on the microSD (`opcache.file_cache`) | in PSRAM (a shared-memory segment) |
| Per request | reloaded from the card, no recompile | served straight from RAM, no recompile, no SD read |
| Cost | one SD read per file; leaves the full PSRAM for the request | fastest, but the cache is reserved out of PSRAM |
| Survives reboot | yes, it is on the card | no, it re-warms on the first request |
| Needs a card | yes, a writable microSD | no |
| Good for | a large framework | a small app whose bytecode and per-request heap both fit in PSRAM |

<!-- @callout variant="info" title="What both modes share" -->
Either way, `validate_timestamps` is off and paths are treated as absolute (`use_cwd=0`), so OPcache
never re-checks a file after it has been cached. The first request warms the cache; every request
after that skips the compiler entirely.
<!-- @endcallout -->

## Enabling it

OPcache is opt-in per project. Turn it on in the project config; the mode is chosen at build time by
the `in_memory` setting.

<!-- @tabs labels="File cache, In-RAM SHM" -->
<!-- @tab index="0" -->

The default. The bytecode is written to a directory on the microSD and reloaded per request, so the
request keeps the whole PSRAM. This is the right choice for a large framework.

<!-- @code-block language="toml" label="php.toml — file cache (default)" -->
```toml
[extensions.opcache]
enabled = true
# in_memory defaults to false: the cache lives on the card
```
<!-- @endcode-block -->

The firmware creates `/sdcard/opcache` on boot (once the card is mounted) and points
`opcache.file_cache` at it. With no card present OPcache is not enabled and the board logs
`opcache: no microSD, not enabled (needs a writable cache dir)`.

<!-- @endtab -->
<!-- @tab index="1" -->

Opt-in with `in_memory`. The compiled bytecode stays in a PSRAM shared-memory segment between
requests, so after warm-up there is neither a recompile nor an SD read. No card is needed. The catch:
the bytecode plus the per-request heap must both fit in PSRAM, so this only suits a small app.

<!-- @code-block language="toml" label="php.toml — in-RAM cache in PSRAM" -->
```toml
[extensions.opcache]
enabled = true
in_memory = true   # keep the cache in PSRAM instead of on the card (small apps only)
```
<!-- @endcode-block -->

<!-- @endtab -->
<!-- @endtabs -->

At the build layer these map to two CMake options: `PHP_EXT_OPCACHE` compiles OPcache in, and
`PHP_EXT_OPCACHE_SHM` (the `in_memory` setting) selects the PSRAM shared-memory backend instead of
the file cache. On boot the firmware logs which mode it took: `opcache: file cache at /sdcard/opcache`
or `opcache: in-RAM (PSRAM SHM) bytecode cache`.

## Why the file cache is the default

The bytecode cache and PHP's per-request heap both live in PSRAM. A framework the size of Laravel
needs roughly 16 to 20 MB of heap per request, and its compiled bytecode is well over 12 MB.
Reserving enough PSRAM to hold the bytecode in RAM leaves too little for the request, so it runs out
of memory. The file cache sidesteps this by keeping the bytecode on the card and leaving the whole
PSRAM for the request. For a small app whose bytecode plus heap fit comfortably, `in_memory` is
faster, since after warm-up it touches neither the compiler nor the SD.

That headroom argument is sharper on the smaller boards. On the ESP32-P4 with 32 MB of PSRAM, a
framework still does not fit in-memory, so it uses the file cache; on the ESP32-S3 with 8 MB, only
genuinely small apps have room for the in-memory cache at all.

<!-- @callout variant="warning" title="The in-RAM cache is reserved up front" -->
`opcache.memory_consumption` (16 MB by default) is carved straight out of the per-request heap budget
before the first request runs. On a 32 MB board that is half your PSRAM gone before PHP starts. This
is why `in_memory` is small-apps-only: a large framework cannot afford both the reserved cache and its
own heap.
<!-- @endcallout -->

## Measured speedup

On the ESP32-P4, the file-cache OPcache takes the
[`laravel-demo-optimized`](https://github.com/php-baremetal/php-esp32/tree/master/examples/laravel-demo-optimized) welcome page from about 12 s to
about 8.4 s per request. What remains is SD reads of the bytecode plus execution; the compile is gone.

| Board | Framework | Without OPcache | With file-cache OPcache |
|---|---|---|---|
| ESP32-P4 (32 MB PSRAM) | Laravel welcome page | ~12 s / request | ~8.4 s / request |

The gain is the compile step, which for a large framework is most of the request. What is left over
is the bytecode read plus execution, so `in_memory` (which also drops the read) only helps further
when the app is small enough to fit.

## Using it in practice

<!-- @steps -->

1. **Warm the cache.** The first request after the cache is empty compiles and writes it, so it is as
   slow as no cache, or a touch slower. Every request after that is fast.
2. **Change code, then invalidate.** `validate_timestamps` is off, so OPcache does not check file
   mtimes and will keep using stale bytecode after you edit the source. Clear the cache to pick up the
   change: delete `/sdcard/opcache` in file mode, or reboot the board in in-memory mode.
3. **Keep a writable card in file mode.** The file cache needs a writable microSD; `in_memory` mode
   needs no card.

<!-- @endsteps -->

<!-- @callout variant="note" title="Why no timestamp checking" -->
Leaving `validate_timestamps` off is deliberate. An mtime check would add a slow FATFS `stat` per file
on every request, and the board has no real-time clock to compare against anyway. The code on the card
is treated as static; you invalidate the cache explicitly when you change it.
<!-- @endcallout -->

## Seeded ini directives

OPcache's directives are `PHP_INI_SYSTEM` and have to be set before the engine starts, so they cannot
come from a runtime `ini_set()`. The firmware seeds them through the embed SAPI's `ini_defaults` hook
in `main.c`, before `php_embed_init()` reads the ini. These are set in both modes:

| Directive | Value | Why |
|---|---|---|
| `opcache.enable` | `1` | Turn OPcache on. |
| `opcache.enable_cli` | `1` | The embed SAPI is CLI-like, so this is what activates it. |
| `opcache.validate_timestamps` | `0` | The code on the card is static; no mtime re-check. |
| `opcache.use_cwd` | `0` | All script paths are absolute. |
| `opcache.file_update_protection` | `0` | The clock sits at the 1970 epoch while the card's files are dated in the future, so the "file too new to cache" guard would otherwise skip every file. Safe because `validate_timestamps` is off. |

The file-cache mode adds:

| Directive | Value | Why |
|---|---|---|
| `opcache.file_cache` | `/sdcard/opcache` | The writable cache directory, created on boot. |
| `opcache.file_cache_only` | `1` | Use the file cache exclusively, no SHM. |
| `opcache.max_accelerated_files` | `20000` | Room for a large framework's file count. |

The in-RAM (`in_memory`) mode adds instead:

| Directive | Value | Why |
|---|---|---|
| `opcache.memory_consumption` | `16` (MB) | PSRAM reserved for the cache, taken from the heap budget. |
| `opcache.interned_strings_buffer` | `2` (MB) | Carved from `memory_consumption`. |
| `opcache.max_accelerated_files` | `4000` | Enough for a small app. |
| `opcache.protect_memory` | `0` | `mprotect` is a no-op on this target. |

## How the port works

OPcache is not an ordinary PHP module. It is a Zend extension that hooks the compiler, and it assumes
a full Unix underneath. The adaptations for this target:

- **Static registration.** There is no `opcache.so` to `dlopen`, and this build sets
  `ZEND_EXTENSIONS_SUPPORT == 0` (no libdl), so `zend_register_extension()` is a no-op. The firmware
  runs OPcache's `zend_extension` startup directly, before `zend_startup_extensions()` (patch
  `0006`, keyed on the `PHP_EXT_OPCACHE_ENABLED` marker). The same patch teaches `accel_find_sapi()`
  the `embed` SAPI, so `opcache.enable_cli=1` turns it on.
- **No RTC.** The board boots at the 1970 epoch while the card's files are dated in the future, so
  OPcache's "file too new to cache" guard would skip every file. The firmware sets
  `opcache.file_update_protection=0`, which is safe because `validate_timestamps` is off as well.
- **POSIX gaps.** picolibc lacks `sys/ipc.h`, `sys/shm.h`, `sys/mman.h` and some symbols (`mmap`,
  `writev`, `setuid`). Stub headers plus weak no-op symbols cover the SHM and restart code paths that
  never run in either cache mode (`compat/opcache_stubs/`, `compat/opcache_posix_stubs.c`); `writev`
  gets a real `write()`-loop implementation, because the file-cache writer uses it.
- **PSRAM shared memory.** The `in_memory` mode needs a shared-memory backend, but there is no `mmap`
  and no System V SHM. Since the firmware is a single process with the engine kept alive across
  requests, "shared" memory is just a plain PSRAM allocation that outlives the request. It is compiled
  in as the `USE_MALLOC_SHM` backend (`compat/shared_alloc_malloc.c`) and patch `0007`, which also
  no-ops the file-lock based locking: one task serves one request at a time.

<!-- @callout variant="info" title="Build flags" -->
The port compiles with `ZEND_ENABLE_STATIC_TSRMLS_CACHE=1` (OPcache's required build flag) and leaves
`HAVE_JIT` undefined, so there is no JIT. In file-cache mode the `shared_alloc_*.c` SHM backends
compile empty and are never used.
<!-- @endcallout -->
