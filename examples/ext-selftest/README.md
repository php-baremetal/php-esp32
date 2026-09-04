# ext-selftest

A self-test for the native extensions. It runs on the board, exercises each extension, and prints
`PASS`/`FAIL` on the serial log, so a regression in the extension layer is easy to spot. It's the
quickest way to confirm a build's extensions actually work on real hardware.

## What it checks

It asserts the shape of the stable extension API:

- **`_available()` on every native extension** (`gpio`, `sys`, `mem`, `store`, `wifi`,
  `s3_onboard_rgb`) returns a bool.
- **`sys_delay()` is canonical; `delay()` is a plain alias** (no deprecation warning), plus `sys`
  info functions (`sys_uptime_ms`, `sys_chip_model`, `sys_mac`, `sys_idf_version`).
- **`sys_*` are canonical for memory** (`sys_psram_free()`, `sys_heap_free()`, …); the unprefixed
  `psram_*` / `heap_*` names still work but are **deprecated** (the test confirms an `E_DEPRECATED`).
- **Typed return values** behave as declared: `mem_get()` returns `mixed`, `wifi_ip()` returns
  `?string`, `store_*` round-trips a value.
- **Argument coercion**: a numeric string passed to an `int` parameter coerces (weak mode).

It adapts to what's compiled in: the opt-in extensions are guarded with `function_exists()`, so it
still runs on a build with fewer of them.

## Build and run

The config targets an ESP32-S3 (so the onboard-RGB extension is available) and enables `wifi`,
`s3_onboard_rgb` and a `store` partition, so every extension is exercised.

```sh
phpflash build && phpflash flash && phpflash monitor
```

Watch the serial output for the `PASS`/`FAIL` lines and the final `N passed, 0 failed`. See
[`monitor.txt`](monitor.txt) for a real run (19/19 on an S3-Zero "Super Mini").
