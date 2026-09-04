# Built-in extension API

The native (C) extensions that come with the firmware and expose the board's hardware and runtime to
PHP. This is the reference for their functions as of the 1.0 API.

Two groups:

- **Always compiled in** — `gpio`, `sys`, `mem`, `store`. Available in every build; no config needed.
- **Opt-in** — `wifi`, `s3_onboard_rgb`. Compiled in only when the project enables them
  (`[extensions.<name>] enabled = true`), because they carry weight or are board-specific.

Every extension exposes a **`<name>_available()`** probe that returns `true` when the extension is
compiled in and its preconditions are met. Because an absent extension's functions don't exist at all,
pair it with `function_exists()` when a build might not include it:

```php
if (function_exists('wifi_available') && wifi_available()) {
    // wifi_* is safe to use
}
```

All functions carry typed signatures (arguments and return types), so they participate in reflection
and the usual PHP type coercion.

---

## gpio — pins and timing

Always compiled in.

```php
gpio_mode(int $pin, int $mode): void      // $mode is GPIO_INPUT or GPIO_OUTPUT
gpio_write(int $pin, int $level): void     // $level: 0 or non-zero
gpio_read(int $pin): int                    // 0 or 1
gpio_available(): bool
```

Constants: `GPIO_INPUT`, `GPIO_OUTPUT`. Timing lives in `sys` — use `sys_delay()` (or its `delay()`
alias) to pause.

```php
gpio_mode(2, GPIO_OUTPUT);
gpio_write(2, 1);
sys_delay(500);   // or delay(500)
```

## mem — in-RAM key/value store (per boot)

Always compiled in. A volatile store shared across the requests of a single boot (the RAM twin of
`store_*`). See [In-RAM store](../storage/in-ram-store.md).

```php
mem_set(string $key, mixed $value): bool          // value is serialized (a copy)
mem_get(string $key, mixed $default = null): mixed
mem_has(string $key): bool
mem_delete(string $key): bool
mem_clear(): bool
mem_keys(): array
mem_available(): bool
```

## store — persistent key/value store (survives reboot)

Always compiled in, but **active only when the project reserves a store partition**
(`[store] size_kb = N`); without it, `store_available()` returns `false` and the setters are inert.
See [Persistent store](../storage/persistent-store.md).

```php
store_set(string $key, string $value): bool           // keys <= 15 chars, values are strings
store_get(string $key, ?string $default = null): ?string
store_has(string $key): bool
store_delete(string $key): bool
store_clear(): bool
store_keys(): array
store_available(): bool
```

## sys — system and runtime

Always compiled in. Timing, reboot and reset info, chip/board identity, and memory introspection.
(The memory functions report the real ESP-IDF heap, which PHP's own `memory_get_usage()` can't on this
port — with `USE_ZEND_ALLOC=0` the Zend heap lives in PSRAM via `malloc`, so it reads 0.)

```php
// timing
sys_delay(int $ms): void       // sleep, yielding the core (never a busy-wait)
sys_uptime_ms(): int           // milliseconds since boot
sys_micros(): int              // microseconds since boot

// control
sys_restart(): void            // reboot the chip (does not return)
sys_reset_reason(): string     // "poweron", "panic", "task_wdt", "deepsleep", ...

// identity
sys_chip_model(): string       // e.g. "ESP32-S3"
sys_cpu_freq_mhz(): int        // configured CPU frequency
sys_mac(): string              // factory base MAC, "aa:bb:cc:dd:ee:ff"
sys_idf_version(): string      // the ESP-IDF version the firmware was built with

// memory
sys_psram_free(): int          // free PSRAM, bytes
sys_psram_size(): int          // total PSRAM pool, bytes
sys_psram_largest_free(): int  // largest contiguous free PSRAM block (fragmentation)
sys_heap_free(): int           // free internal RAM, bytes
sys_heap_size(): int           // total internal RAM pool, bytes

sys_available(): bool
```

`delay(int $ms): void` is a plain alias of `sys_delay()`, kept for the Arduino-style idiom. The
unprefixed `psram_free()`, `psram_size()`, `psram_largest_free()`, `heap_free()`, `heap_size()` still
work but are **deprecated** (they emit `E_DEPRECATED`); use the `sys_` names.

## wifi — scan, join, or create a network

Opt-in (`[extensions.wifi] enabled = true`), on WiFi-capable SoCs (ESP32 / ESP32-S3 / C-series, or an
ESP32-P4 with a companion over ESP-HOSTED). Credentials are passed at runtime, never baked in.

```php
// station (join a network)
wifi_scan(): array|false          // [{ssid, bssid, rssi, channel, auth}, ...], or false on a scan error
wifi_connect(string $ssid, ?string $password = null, int $timeout_ms = 15000): bool
wifi_disconnect(): bool
wifi_connected(): bool
wifi_ip(): ?string                // the STA IP, or null if not connected
wifi_rssi(): ?int                 // signal in dBm, or null

// access point (create a network)
wifi_ap_start(string $ssid, ?string $password = null, int $channel = 1, int $max_conn = 4): bool
wifi_ap_stop(): bool
wifi_ap_ip(): ?string             // the AP IP (default 192.168.4.1), or null
wifi_ap_clients(): int            // number of connected stations

wifi_available(): bool
```

`wifi_scan()` returns `false` on a scan failure (not an empty array), so guard it:

```php
$aps = wifi_scan();
foreach ($aps ?: [] as $ap) { /* ... */ }
```

## s3_onboard_rgb — the onboard RGB LED

Opt-in (`[extensions.s3_onboard_rgb] enabled = true`), **ESP32-S3 only** — the addressable WS2812 LED
soldered onto S3 dev boards. The data pin is set at build time (`[extensions.s3_onboard_rgb] pin = N`,
default 48).

```php
s3_onboard_rgb_set(int $r, int $g, int $b): void   // each 0..255
s3_onboard_rgb_hsv(int $h, int $s, int $v): void   // h 0..359, s/v 0..255
s3_onboard_rgb_off(): void
s3_onboard_rgb_available(): bool
```

Constant: `S3_ONBOARD_RGB_PIN` (the data GPIO the build was configured with).

---

To confirm a build's extensions actually work on real hardware, flash the
[`ext-selftest`](../../examples/ext-selftest/) example: it exercises every function above and reports
PASS/FAIL on the serial log.
