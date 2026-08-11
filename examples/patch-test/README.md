# patch-test

A regression test for the vendored port patches. It exercises the runtime behaviour each patch
enables or fixes, so bumping the PHP version can be checked at a glance: build it, flash it, read the
serial log. Every check prints `PASS`, `FAIL`, or `SKIP` (when its extension or a resource such as a
microSD is not present), ending with a count.

The patches it covers, from `components/php/versions/<version>/patches/php/`:

| Patch | What it does | Checked by |
|---|---|---|
| `0001` closure run-time-cache arena | Allocates a scope-bound closure's run-time cache from the request arena, so freeing it no longer corrupts the PSRAM heap. | scope-bound closures, `Closure::bind`, 3000 closures in a loop |
| `0002` ext/date optional minimal tz | Makes the timezone database swappable (full vs UTC-only). | `DateTime` with the named zone `Europe/Rome` and its DST, interval math |
| `0003` mbstring optional no-CJK | Makes the legacy CJK codecs optional. | `mb_strlen`, a UTF-8 to Shift-JIS round trip, `mb_convert_kana`, `mb_ereg` |
| `0004` csprng esp getrandom | Routes `random_int` and `random_bytes` to the hardware RNG (no `/dev/urandom` here). | `random_int`, `random_bytes`, entropy |
| `0005` session files no-cloexec warn | Stops the files save handler printing a bogus `fcntl(F_SETFD)` warning the FATFS VFS triggers. | a `session_start` round trip on the microSD, asserting no such warning |
| `0006` opcache static embed | Links OPcache statically and accepts the `embed` SAPI, so it registers and enables. | `opcache_get_status()` reports enabled |
| `0007` opcache malloc SHM backend | A PSRAM-backed shared-memory segment for the in-memory cache mode. | `opcache_get_status()` reports `used_memory > 0` |

## Two builds cover all seven

Patch `0001` only takes its patched path when OPcache is **off** (with OPcache on, the closure
run-time cache comes from its shared memory instead). Patches `0006`/`0007` need OPcache **on**. So the
full set is covered by running the same test in two configurations:

1. **As shipped (OPcache off).** Tests `0001` on its real path, plus `0002`, `0003`, `0004`, `0005`.
   The OPcache section reports `SKIP`.
2. **With in-memory OPcache.** Add to the config, then rebuild:

   ```toml
   [extensions.opcache]
   enabled   = true
   in_memory = true
   ```

   Now the OPcache section tests `0006` and `0007`. The rest still pass (the closure checks stay
   correct; they just no longer exercise the `0001` arena path).

## Run it

```sh
phpflash build
phpflash flash
phpflash monitor
```

It is an `embedded` project, so the source is baked into the image and it needs no card to run, with
one exception: the `0005` session check writes to a microSD (mounted alongside via
`[storage] microsd = true`). Without a card that one check reports `SKIP`; everything else runs.

The board is the ESP32-P4-Pico by default. To run it on another board, change `[board] target`. The
non-hardware checks are architecture-independent, so the same suite passes on the ESP32-S3.
