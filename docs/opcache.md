# The `opcache` extension: Zend OPcache on the ESP32

Without an opcode cache, PHP recompiles every script it runs on every request: tokenize, parse,
compile, optimize. For a single small script that's cheap; for a framework that pulls in hundreds of
files (Laravel, Symfony, …) it dominates the request time. Zend OPcache caches the **compiled
bytecode** so the next run skips all of that.

This is a real port of the bundled `ext/opcache` (not a stub), verified on ESP32-P4 hardware. It is
built **without JIT** (unsupported on RISC-V) and in one of two cache modes.

## Two cache modes

| | **file cache** (default) | **in-memory** (`in_memory` setting) |
|---|---|---|
| Bytecode lives | on the microSD (`opcache.file_cache`) | in PSRAM (a shared-memory segment) |
| Per request | reloads bytecode from the card (no recompile) | served straight from RAM (no recompile, no SD read) |
| Cost | one SD read per file; leaves the full PSRAM for the request | fastest, but the cache is reserved out of the 32 MB PSRAM |
| Survives reboot | yes (it's on the card) | no (RAM) — re-warms on first request |
| Good for | a large framework (Laravel) | a small app whose bytecode **and** per-request heap fit in PSRAM |

Enable it in the project config:

```toml
[extensions.opcache]
enabled = true
# in_memory = true   # keep the cache in PSRAM instead of on the card (small apps only)
```

### Why file cache is the default

The bytecode cache and PHP's per-request heap both live in the 32 MB PSRAM. A framework the size of
Laravel needs ~16–20 MB of heap **per request**, and its compiled bytecode is well over 12 MB.
Reserving enough PSRAM to hold the bytecode in RAM leaves too little for the request, so it runs out
of memory. The file cache sidesteps this by keeping the bytecode on the card, leaving the whole PSRAM
free for the request. For a **small** app (bytecode + heap comfortably under 32 MB), `in_memory` is
faster — after warm-up it touches neither the compiler nor the SD.

On the ESP32-P4, file-cache OPcache takes the [`laravel-demo-optimized`](../examples/laravel-demo-optimized/)
welcome page from ~12 s to **~8.4 s** per request (the remaining time is SD reads of the bytecode plus
execution — the compile is gone).

## Using it

- The first request after the cache is empty **warms it** (compiles + writes the cache), so it's as
  slow as — or slower than — no cache. Every request after that is fast.
- `validate_timestamps` is **off**: OPcache does not check file mtimes, so it will keep using stale
  bytecode after you change the code. **Invalidate by clearing the cache**: delete `/sdcard/opcache`
  (file mode) or reboot the board (in-memory mode). This is deliberate — mtime checks would add a slow
  FATFS `stat` per file, and the board has no real-time clock anyway (see below).
- The cache needs a writable microSD in file mode; `in_memory` mode needs no card.

## How the port works

OPcache isn't a normal PHP module — it's a *Zend extension* that hooks the compiler, and it assumes a
full Unix underneath. The adaptations for this target:

- **Static registration.** There's no `opcache.so` to `dlopen`, and this build has
  `ZEND_EXTENSIONS_SUPPORT == 0` (no libdl), so `zend_register_extension()` is a no-op. The firmware
  runs OPcache's `zend_extension` startup directly, before `zend_startup_extensions()`
  (patch `0006`). The same patch teaches `accel_find_sapi()` the `embed` SAPI, so
  `opcache.enable_cli=1` turns it on.
- **No RTC.** The board boots at the 1970 epoch while the card's files are dated in the "future", so
  OPcache's "file too new to cache" guard would skip every file. The firmware sets
  `opcache.file_update_protection=0` (safe, since `validate_timestamps` is off too).
- **POSIX gaps.** picolibc lacks `sys/ipc.h` / `sys/shm.h` / `sys/mman.h` and some symbols
  (`mmap`, `writev`, `setuid`, …). Stub headers plus weak no-op symbols cover the SHM/restart code
  paths that never run in either cache mode (`compat/opcache_stubs/`, `compat/opcache_posix_stubs.c`);
  `writev` is given a real `write()`-loop implementation because the file-cache writer uses it.
- **PSRAM shared memory.** The `in_memory` mode needs a shared-memory backend, but there's no
  `mmap`/System V SHM. Since the firmware is a single process with the engine kept alive across
  requests, "shared" memory is just a plain PSRAM allocation that outlives the request — see
  `compat/shared_alloc_malloc.c` and patch `0007` (which also no-ops the file-lock based locking:
  one task serves one request at a time).

The ini is seeded through the embed SAPI's `ini_defaults` hook in `main.c` (OPcache's directives are
`PHP_INI_SYSTEM`, so they must be set before startup).
