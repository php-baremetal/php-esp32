# store-demo

A boot counter that survives resets, kept in the reboot-persistent store (`store_*`, backed by NVS).
Flash it, then reset the board a few times and watch the count climb.

## What it shows

```php
$boots = (int) store_get('boots', '0') + 1;   // read the previous value (a string)
store_set('boots', (string) $boots);          // write it back -- persisted immediately
```

`setup()` bumps a counter and prints it, writes a one-time message on the first boot, and lists the
stored keys with `store_keys()`:

```
=== store-demo :: persistent boot counter ===
  boot count : 4   (survives resets)
  first_msg  : hello from boot #1
  keys       : boots, first_msg
```

The value comes back unchanged across every reset -- it lives in flash, not in RAM.

## Enabling the store

The store needs a slice of flash, configured in `php-esp32.config.toml`:

```toml
[store]
size_kb = 32     # size of the persistent NVS partition; 0 or absent = no persistence
```

Without it, `store_available()` returns `false` and the other functions do nothing. See
[docs/store.md](../../docs/store.md) for the full API, the key/value limits, and how it differs from
the build-time [`.env`](../env-demo/).

No wiring, no card, no network: `storage_type` is `embedded`, so the script runs from flash and the
counter persists in the on-chip `phpstore` partition.
