# Persistent store (`store_*`)

A small key-value store that survives resets. Write a value from PHP and it comes back on the next
boot -- a boot counter, a last-known state, a provisioning flag, a calibration value:

```php
$boots = (int) store_get('boots', '0') + 1;
store_set('boots', (string) $boots);
```

It is backed by the SoC's **NVS** (Non-Volatile Storage): a wear-levelled, power-loss-safe key-value
area in flash. The [`store-demo`](../examples/store-demo/) example is a boot counter you can watch
climb across resets.

## Enabling it

The store needs a slice of flash, so it is **off by default**. Give it a size in the project config:

```toml
[store]
size_kb = 32     # size of the persistent NVS partition; 0 or absent = no persistence
```

`phpflash` passes this to the build, and the partition generator adds a dedicated `phpstore` NVS
partition (`cmake/gen-partitions.cmake`). With no `[store]` (or `size_kb = 0`) there is no partition
and `store_available()` returns `false`; the other functions are inert (no error). 32 KB holds a few
hundred small entries; NVS keeps some pages for bookkeeping, so the floor is 16 KB.

## API

| Function | Returns | Notes |
|---|---|---|
| `store_set(string $key, string $value)` | `bool` | Persist a value; auto-committed. |
| `store_get(string $key, ?string $default = null)` | `?string` | The value, or `$default` (or `null`) if absent. |
| `store_has(string $key)` | `bool` | |
| `store_delete(string $key)` | `bool` | |
| `store_clear()` | `bool` | Wipe every key. |
| `store_keys()` | `array` | The keys currently stored. |
| `store_available()` | `bool` | Is persistence configured and ready? |

- **Keys** are at most **15 characters** (an NVS limit); a longer key is rejected (`store_set`
  returns `false`).
- **Values** are strings. Store a number as `(string)` and read it back with `(int)` / `(float)`;
  store a structure with `json_encode()` and read it with `json_decode()`. NVS caps a single string
  near 4 KB.
- Writes are committed immediately, so a value is safe across a power cut the moment `store_set`
  returns.

## What it is for

Configuration and state that must persist: counters, flags, the last reading, a device identity.
NVS is wear-levelled but flash still wears, so it is **not** a log for high-frequency writes -- do not
`store_set` on every loop tick. For bulk or streaming data use the microSD.

The store is separate from the `[env]` build-time environment: `.env` is read-only configuration
compiled in at build time, while the store is written by the running script and changes at runtime.

## How it works

The `store` extension is built into the firmware. At startup it opens the `phpstore` NVS partition
(formatting it on first boot); if the partition isn't there, it stays inert. Keys live in a single
NVS namespace, so `store_clear()` and `store_keys()` see exactly what the script wrote.
