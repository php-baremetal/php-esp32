---
eyebrow: 'Docs · Storage & state'
lede: 'A reboot-persistent key-value store for PHP, backed by the SoC''s NVS flash. Write a value in one boot and read it back in the next — boot counters, provisioning flags, calibration constants, last-known state.'
see_also:
  - href: ./in-ram-store.md
    meta: 'Storage & state'
    label: 'In-RAM store (mem_*)'
  - href: ./environment.md
    meta: 'Storage & state'
    label: 'Build-time environment (.env)'
  - href: ../getting-started/architecture.md
    meta: 'Getting started'
    label: 'Firmware architecture'
prev:
  label: 'Custom C extensions'
  href: ../extensions/custom-extensions.md
next:
  label: 'In-RAM store'
  href: ./in-ram-store.md
---

# Persistent store (`store_*`)

`store_*` is a small key-value store that survives resets. Write a value from PHP and it comes back on the next boot — a boot counter, a last-known state, a provisioning flag, a calibration value:

<!-- @code-block language="php" label="A value that survives a reset" -->
```php
$boots = (int) store_get('boots', '0') + 1;
store_set('boots', (string) $boots);
```
<!-- @endcode-block -->

It is backed by the SoC's **NVS** (Non-Volatile Storage): a wear-levelled, power-loss-safe key-value area in flash. The store is written by the running script and changes at runtime — it is the read-write counterpart to the read-only, build-time [`.env`](./environment.md) environment, and the reboot-surviving counterpart to the volatile in-RAM [`mem_*`](./in-ram-store.md) table.

The [`store-demo`](https://github.com/php-baremetal/php-esp32/tree/master/examples/store-demo) example is a boot counter you can watch climb across resets.

## Enabling it

The store needs a slice of flash, so it is **off by default**. Give it a size in the project config:

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
[store]
size_kb = 32     # size of the persistent NVS partition; 0 or absent = no persistence
```
<!-- @endcode-block -->

`phpflash` passes this size into the build as `-DPHP_STORE_KB`, and the partition generator (`cmake/gen-partitions.cmake`) adds a dedicated `phpstore` NVS partition to the generated table. This is independent of the embedded-vs-microSD choice: a project can run its source from flash or from an SD card and still have a persistent store either way.

With no `[store]` section (or `size_kb = 0`) no partition is generated, `store_available()` returns `false`, and the other functions are inert — they return `false`/`null`/`[]` rather than raising an error, so the same script runs unchanged with or without persistence configured.

### How the partition is sized

The requested `size_kb` is rounded up to NVS's 4 KB page alignment and then floored at 16 KB — NVS needs a few pages for bookkeeping before it holds any user data, so anything smaller is bumped up to that floor. A 32 KB partition holds a few hundred small entries.

| `size_kb` in config | Effect |
|---|---|
| absent, or `0` | No `phpstore` partition; `store_available()` is `false`; all writes/reads inert. |
| `1`–`16` | Rounded up to the **16 KB** minimum (NVS needs the headroom). |
| `> 16` | Rounded up to the next 4 KB boundary and used as-is. |

The build log prints the resolved size, e.g. `php-esp32: persistent 'phpstore' NVS partition = 32K`.

## API

The `store` extension is built into the firmware and exposes seven functions in the global namespace:

| Function | Returns | Notes |
|---|---|---|
| `store_set(string $key, string $value)` | `bool` | Persist a value; auto-committed on return. |
| `store_get(string $key, ?string $default = null)` | `?string` | The value, or `$default` (or `null`) if absent. |
| `store_has(string $key)` | `bool` | Whether the key currently exists. |
| `store_delete(string $key)` | `bool` | Remove one key; auto-committed. |
| `store_clear()` | `bool` | Wipe every key in the store. |
| `store_keys()` | `array` | The list of keys currently stored. |
| `store_available()` | `bool` | Is persistence configured and ready? |

### `store_set`

<!-- @method name="store_set" returns="bool" visibility="public" / -->

Writes `$value` under `$key` and commits it to flash immediately, so the value is durable the moment the call returns — safe even across an abrupt power cut. Returns `false` if persistence is not configured, if the key is empty or longer than 15 characters, or if the underlying NVS write fails.

<!-- @params -->
<!-- @param name="$key" type="string" required -->
The entry name. At most 15 characters (an NVS limit); a longer or empty key is rejected.
<!-- @endparam -->
<!-- @param name="$value" type="string" required -->
The value to persist. Values are strings; NVS caps a single string near 4 KB.
<!-- @endparam -->
<!-- @endparams -->

### `store_get`

<!-- @method name="store_get" returns="?string" visibility="public" / -->

Reads the value stored under `$key`. Returns the stored string, or `$default` when the key is absent, when persistence is not configured, or when the key is invalid. With no `$default` given, a missing key yields `null`.

<!-- @params -->
<!-- @param name="$key" type="string" required -->
The entry name to read.
<!-- @endparam -->
<!-- @param name="$default" type="?string" -->
Returned verbatim when the key is not present. Defaults to `null`.
<!-- @endparam -->
<!-- @endparams -->

### `store_has`

<!-- @method name="store_has" returns="bool" visibility="public" / -->

Reports whether `$key` currently exists in the store. Returns `false` for a missing key, an invalid key, or when persistence is not configured.

<!-- @params -->
<!-- @param name="$key" type="string" required -->
The entry name to test.
<!-- @endparam -->
<!-- @endparams -->

### `store_delete`

<!-- @method name="store_delete" returns="bool" visibility="public" / -->

Removes `$key` and commits the change to flash. Returns `false` if the key is invalid, if it does not exist, or if persistence is not configured.

<!-- @params -->
<!-- @param name="$key" type="string" required -->
The entry name to remove.
<!-- @endparam -->
<!-- @endparams -->

### `store_clear`, `store_keys`, `store_available`

<!-- @method name="store_clear" returns="bool" visibility="public" / -->

Erases every key in the store's namespace and commits. Returns `false` when persistence is not configured.

<!-- @method name="store_keys" returns="array" visibility="public" / -->

Returns a list of the keys currently stored — exactly the keys the script wrote, since all entries live in a single dedicated NVS namespace. Returns an empty array when the store is empty or unconfigured.

<!-- @method name="store_available" returns="bool" visibility="public" / -->

Returns `true` when a `phpstore` partition exists and was opened successfully at boot. Use it to branch on whether persistence is present before relying on the other calls.

## Keys, values, and durability

- **Keys** are at most **15 characters** — an NVS constraint (`NVS_KEY_NAME_MAX_SIZE` is 16 including the terminator). A longer or empty key is rejected and `store_set` returns `false`.
- **Values** are strings. Store a number as `(string)` and read it back with `(int)` / `(float)`; store a structure with `json_encode()` and read it with `json_decode()`. NVS caps a single string near **4 KB**.
- **Writes commit immediately.** Both `store_set` and `store_delete` call `nvs_commit()` before returning, so a value is durable the instant the call returns — there is no separate flush step and no data loss window on power failure.
- **One namespace.** Every entry lives in a single NVS namespace inside the `phpstore` partition, so `store_keys()` and `store_clear()` see precisely what the script wrote and nothing from the system's own NVS.

<!-- @code-block language="php" label="Numbers and structures round-trip as strings" -->
```php
// a number
store_set('threshold', (string) 42);
$threshold = (int) store_get('threshold', '0');

// a small structure
store_set('cfg', json_encode(['ssid' => 'lab', 'ch' => 6]));
$cfg = json_decode(store_get('cfg', '{}'), true);
```
<!-- @endcode-block -->

## Example: a boot counter

The `store-demo` example bumps a counter on every boot, writes a one-time message on the first boot, and lists the stored keys — the classic demonstration that the value lives in flash rather than RAM:

<!-- @code-block language="php" label="project-src/index.php" -->
```php
function setup(): void
{
    echo "\n=== store-demo :: persistent boot counter ===\n";
    if (!store_available()) {
        echo "  persistence is OFF -- add [store] size_kb to php-esp32.config.toml\n";
        echo "=============================================\n";
        return;
    }

    $boots = (int) store_get('boots', '0') + 1;   // values are strings; cast to use them
    store_set('boots', (string) $boots);
    if ($boots === 1) {
        store_set('first_msg', 'hello from boot #1');
    }

    printf("  boot count : %d   (survives resets)\n", $boots);
    printf("  first_msg  : %s\n", store_get('first_msg', '(none)'));
    printf("  keys       : %s\n", implode(', ', store_keys()));
    echo "=============================================\n";
}
```
<!-- @endcode-block -->

Flash it, reset the board a few times, and the count climbs across every reset:

<!-- @code-block language="text" label="Serial output after four boots" -->
```
=== store-demo :: persistent boot counter ===
  boot count : 4   (survives resets)
  first_msg  : hello from boot #1
  keys       : boots, first_msg
```
<!-- @endcode-block -->

Note the guard on `store_available()`: the demo runs even without persistence configured, printing a hint instead of failing. Building `store_available()` checks into any script that uses the store keeps it portable between builds that ship a `[store]` partition and builds that don't.

## What it is — and is not — for

Use the persistent store for **configuration and state that must survive a reboot**: counters, feature flags, the last reading, a device identity, a provisioning token, a calibration constant. These are written rarely and read often, which is exactly the access pattern flash is good at.

<!-- @callout variant="warning" title="Not a log for high-frequency writes" -->
NVS is wear-levelled, but flash cells still wear out with each erase/write cycle. Do **not** `store_set` on every loop tick or every HTTP request — a hot write loop will eventually exhaust the partition. For counters, caches, and rate-limit buckets that change constantly, write to the volatile in-RAM [`mem_*`](./in-ram-store.md) table instead: it touches no flash and is designed to be written on every request. For bulk or streaming data (readings, files, a database), use the microSD. Reserve `store_*` for values that genuinely need to persist and change slowly.
<!-- @endcallout -->

### How it relates to the other state stores

| You want to keep... | Use | Survives reboot? | Touches flash? |
|---|---|---|---|
| Config/state written rarely, read often | **`store_*`** (this page) | Yes | Yes (wear-levelled) |
| A volatile scalar/array written every request | [`mem_*`](./in-ram-store.md) | No | No |
| Read-only config baked in at build time | [`.env`](./environment.md) | Yes (immutable) | Compiled into firmware |
| Bulk / relational / streaming data | microSD (files, SQLite via PDO) | Yes | SD card |

## How it works

The `store` extension is compiled into the firmware and initialised in its module startup. At boot it opens the `phpstore` NVS partition; on a brand-new or version-mismatched partition it erases and re-formats it once, then opens it. If the partition is not present at all — persistence not configured — initialisation stays quiet and inert, and every function short-circuits on an internal readiness flag (surfaced to PHP as `store_available()` returning `false`).

All entries are stored as NVS strings in a single namespace within that partition, which is why `store_keys()` enumerates exactly the script's own keys and `store_clear()` wipes only them, leaving the system NVS untouched.
