# The opcache extension: Zend OPcache on the ESP32

Without an opcode cache, PHP recompiles every script on every request: tokenize, parse, compile,
optimize. For one small script that is cheap. For a framework that pulls in hundreds of files it
dominates the request time. Zend OPcache caches the compiled bytecode so the next run skips all of
that.

This is a real port of the bundled `ext/opcache`, not a stub, verified on ESP32-P4 hardware. It is
built without JIT (unsupported on these targets) and runs in one of two cache modes.

## Two cache modes

| | file cache (default) | in-memory (`in_memory` setting) |
|---|---|---|
| Bytecode lives | on the microSD (`opcache.file_cache`) | in PSRAM (a shared-memory segment) |
| Per request | reloaded from the card, no recompile | served straight from RAM, no recompile, no SD read |
| Cost | one SD read per file; leaves the full PSRAM for the request | fastest, but the cache is reserved out of PSRAM |
| Survives reboot | yes, it is on the card | no, it re-warms on the first request |
| Good for | a large framework | a small app whose bytecode and per-request heap both fit in PSRAM |

Enable it in the project config:

```toml
[extensions.opcache]
enabled = true
# in_memory = true   # keep the cache in PSRAM instead of on the card (small apps only)
```

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

On the ESP32-P4, the file-cache OPcache takes the
[`laravel-demo-optimized`](../examples/laravel-demo-optimized/) welcome page from about 12 s to about
8.4 s per request. What remains is SD reads of the bytecode plus execution; the compile is gone.

## Using it

- The first request after the cache is empty warms it (compiles and writes the cache), so it is as
  slow as no cache, or a touch slower. Every request after that is fast.
- `validate_timestamps` is off, so OPcache does not check file mtimes and will keep using stale
  bytecode after you change the code. Invalidate by clearing the cache: delete `/sdcard/opcache` in
  file mode, or reboot the board in in-memory mode. This is deliberate. An mtime check would add a
  slow FATFS `stat` per file, and the board has no real-time clock to compare against anyway.
- The cache needs a writable microSD in file mode; `in_memory` mode needs no card.

## How the port works

OPcache is not an ordinary PHP module. It is a Zend extension that hooks the compiler, and it assumes
a full Unix underneath. The adaptations for this target:

- **Static registration.** There is no `opcache.so` to `dlopen`, and this build sets
  `ZEND_EXTENSIONS_SUPPORT == 0` (no libdl), so `zend_register_extension()` is a no-op. The firmware
  runs OPcache's `zend_extension` startup directly, before `zend_startup_extensions()` (patch
  `0006`). The same patch teaches `accel_find_sapi()` the `embed` SAPI, so `opcache.enable_cli=1`
  turns it on.
- **No RTC.** The board boots at the 1970 epoch while the card's files are dated in the future, so
  OPcache's "file too new to cache" guard would skip every file. The firmware sets
  `opcache.file_update_protection=0`, which is safe because `validate_timestamps` is off as well.
- **POSIX gaps.** picolibc lacks `sys/ipc.h`, `sys/shm.h`, `sys/mman.h` and some symbols (`mmap`,
  `writev`, `setuid`). Stub headers plus weak no-op symbols cover the SHM and restart code paths that
  never run in either cache mode (`compat/opcache_stubs/`, `compat/opcache_posix_stubs.c`); `writev`
  gets a real `write()`-loop implementation, because the file-cache writer uses it.
- **PSRAM shared memory.** The `in_memory` mode needs a shared-memory backend, but there is no `mmap`
  and no System V SHM. Since the firmware is a single process with the engine kept alive across
  requests, "shared" memory is just a plain PSRAM allocation that outlives the request. See
  `compat/shared_alloc_malloc.c` and patch `0007`, which also no-ops the file-lock based locking:
  one task serves one request at a time.

The ini is seeded through the embed SAPI's `ini_defaults` hook in `main.c`, because OPcache's
directives are `PHP_INI_SYSTEM` and have to be set before startup.
