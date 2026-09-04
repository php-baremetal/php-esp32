---
eyebrow: 'Docs · Storage & state'
lede: 'A volatile in-RAM key-value store (mem_*) that hands data from one HTTP request to the next, paired with a one-time init script that runs once before the web server starts. Together they answer "how do I set up once and share the result" in the shared-nothing web-server model.'
see_also:
  - href: './persistent-store.md'
    meta: 'Storage & state'
    label: 'Persistent store (store_*)'
  - href: '../getting-started/architecture.md'
    meta: 'Getting started'
    label: 'Execution models'
  - href: '../extensions/custom-extensions.md'
    meta: 'Extensions'
    label: 'Custom C extensions'
prev:
  label: 'Persistent store'
  href: './persistent-store.md'
next:
  label: 'Build-time environment'
  href: './environment.md'
---

# In-RAM store (`mem_*`) and the web-server init script

In the **web-server** execution model every HTTP request is a fresh PHP request cycle. The engine
brings userland up, runs the entry script, and tears everything back down — variables, objects and
resources are gone when the request ends. It is shared-nothing, exactly like PHP-FPM behind nginx:
nothing you assign in one request is visible in the next.

That leaves a gap. A device serving HTTP usually has setup it wants to do **once** — bring a display
up, read a calibration value, compute a boot-time configuration — and data it wants to **share** —
a counter, a small cache, a value seeded at boot. Two features fill that gap, and this page covers
both because they are meant to be used together:

- **`mem_*`** — a volatile in-RAM key-value store, shared across all the requests of a single boot.
- **`[web-server] init`** — a PHP script the firmware runs once, before the HTTP server starts.

One holds the data; the other produces it. In the `init-loop` and `event-driven` models the script
already runs exactly once and keeps its own state in scope, so neither feature is usually needed —
they exist for the request-per-request web-server model where userland cannot persist on its own.

## What survives a request, and what does not

The engine tears down userland at the end of every request. What persists is everything **below**
PHP: C-extension state, mounted filesystems, the NVS-backed `store_*`, and the in-RAM `mem_*` table.
Deciding where a piece of shared state belongs is the whole design problem, and it comes down to two
questions: is the thing **data** (serializable) or a **live handle** (a resource), and must it
survive a **reboot**?

| You want to share… | Put it in… | Survives reboot? |
|---|---|---|
| a scalar / array / serializable object, volatile | **`mem_*`** (RAM) | no — wiped on reboot |
| config read often, written rarely | **`store_*`** (NVS flash) — see [persistent-store.md](./persistent-store.md) | yes |
| a **live handle** — a display, a socket, a bus | a **C extension** — see [custom-extensions.md](../extensions/custom-extensions.md) | n/a — brought up at boot |
| large or relational data | SQLite on the microSD (PDO) | yes |

The distinction that trips people up is **data versus live handle**. `mem_*` and `store_*` both work
by serializing a value, so anything you put in them must survive a round-trip through PHP's
serializer. A live resource — an open socket, an initialised display driver, a file handle — cannot
be serialized and cannot be kept alive across a request teardown. Those belong in a C extension,
where the handle lives in C memory below PHP and userland reaches it through the extension's
functions. The init script brings the handle up once; every request calls into the extension to use
it.

<!-- @callout variant="note" title="Data goes in mem_*/store_*; handles go in a C extension" -->
If you can `serialize()` it, it can live in `mem_*` (volatile) or `store_*` (persistent). If it is
a live resource, it cannot — put it in a C extension and let the init script initialise it once.
This split is why the paired example below seeds a *value* into `mem_*` but would bring a *display*
up through an extension.
<!-- @endcallout -->

## `mem_*` — a volatile in-RAM store

`mem_*` is the RAM twin of `store_*`. It is a string-keyed table that lives in persistent
(non-request) memory, so a value written in one request is still there in the next. It is **wiped on
reboot** and touches **no flash**, which is the property that matters most: because there is no flash
wear, it is fine to write on **every** request — a hit counter, a small cache, a rate-limit bucket —
where writing `store_*` that often would grind the flash down.

<!-- @code-block language="php" label="A per-request counter kept in RAM" -->
```php
// per request: count requests in RAM, no flash wear
$hits = (int) mem_get('hits', 0) + 1;
mem_set('hits', $hits);
echo "request #$hits";
```
<!-- @endcode-block -->

`mem_*` is **always built in**. There is no configuration key, no partition to size, and nothing to
enable — it is present in every firmware. Because the web server processes requests one at a time,
the table needs no locking and is not thread-safe by design.

### How values are stored

A value is stored by running PHP's serializer over it and keeping the resulting bytes in a
persistent `zend_string` owned by the table. `mem_get` does the reverse: it finds the blob and
unserializes it into a **fresh, independent copy** for the current request. There is no shared live
object — an object graph cannot outlive the request that built it — so the model is APCu's, not a
shared-memory pointer:

- **Scalars, arrays and serializable objects** all work, since they all serialize cleanly.
- Each `mem_get()` hands back a **copy**. Mutating that copy changes nothing in the store until you
  `mem_set()` it back.
- A closure, a PDO connection, a stream, or any object that refuses to serialize cannot be stored.

<!-- @callout variant="warning" title="Each mem_get is a copy, not a shared live object" -->
`mem_get('cart')` returns a fresh deserialized copy every call. Two requests that both read the same
key get two independent values. To publish a change you must **read, mutate, then `mem_set()` back** —
there is no live object that several requests share by reference. If a stored value fails to
unserialize (corrupt or partial), `mem_get` discards it and returns the default instead.
<!-- @endcallout -->

### API

Every function operates on the single process-wide table. Keys are arbitrary strings; an empty key
is rejected by the writers.

<!-- @method name="mem_set" returns="bool" visibility="public" / -->

Serialize `$value` and store it under `$key`, replacing any previous value for that key.

<!-- @params -->
<!-- @param name="$key" type="string" required -->
The key to store under. An empty string is rejected and the call returns `false`.
<!-- @endparam -->
<!-- @param name="$value" type="mixed" required -->
Any serializable value — scalar, array, or serializable object. Stored as a serialized copy, not a
live reference.
<!-- @endparam -->
<!-- @endparams -->

Returns `true` on success; `false` if the key is empty or the value could not be serialized.

<!-- @method name="mem_get" returns="mixed" visibility="public" / -->

Read the value stored under `$key`, returning a fresh independent copy.

<!-- @params -->
<!-- @param name="$key" type="string" required -->
The key to read.
<!-- @endparam -->
<!-- @param name="$default" type="mixed" -->
Returned when the key is absent — or when its stored bytes fail to unserialize. Defaults to `null`.
<!-- @endparam -->
<!-- @endparams -->

Returns a fresh copy of the stored value, or `$default` (or `null`) if the key is missing.

<!-- @method name="mem_has" returns="bool" visibility="public" / -->

Report whether `$key` currently exists in the store. Returns `false` for an empty key.

<!-- @method name="mem_delete" returns="bool" visibility="public" / -->

Remove `$key` from the store. Returns `true` if the key existed and was removed, `false` otherwise
(including for an empty key).

<!-- @method name="mem_clear" returns="bool" visibility="public" / -->

Drop every key in the store. Returns `true`.

<!-- @method name="mem_keys" returns="array" visibility="public" / -->

Return a list of the keys currently stored, as a plain indexed array of strings. Returns an empty
array when the store is empty.

The same surface at a glance:

| Function | Returns | Notes |
|---|---|---|
| `mem_set(string $key, mixed $value)` | `bool` | Store a value (a serialized copy). Empty key or unserializable value → `false`. |
| `mem_get(string $key, mixed $default = null)` | `mixed` | A fresh copy, or `$default` (or `null`) if absent or corrupt. |
| `mem_has(string $key)` | `bool` | Key present? Empty key → `false`. |
| `mem_delete(string $key)` | `bool` | `true` if the key existed and was removed. |
| `mem_clear()` | `bool` | Drop every key; always `true`. |
| `mem_keys()` | `array` | The keys currently stored. |
| `mem_available()` | `bool` | Whether the in-RAM store is up (always `true` once the module started). |

### Lifecycle and internals

The table is a `HashTable` allocated with the persistent flag at module init, so it (and its keys
and buckets) outlive any single request and survive `php_request_shutdown()`. Values are persistent
`zend_string`s freed by a destructor when a key is overwritten, deleted, or the table is cleared.
The table is destroyed only at module shutdown — in practice, at reboot. Nothing here writes flash,
so the store is genuinely free to write as often as you like within a boot; the trade-off is simply
that everything is gone the moment the board resets.

## `[web-server] init` — run something once before serving

A web-server project can name a PHP script to run **once**, after the engine starts and before the
HTTP server accepts its first connection. This is where one-time setup belongs: bringing hardware up
through a C extension, seeding `mem_*`, or reading `store_*` into a shape requests can use. The
script's output goes to the **serial console**, not to any HTTP response — at the point it runs,
output has not yet been redirected into a request.

You wire it up in the project config. The key is `init` under the `[web-server]` table, a path
relative to the `[php] src` source root:

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
type = "web-server"

[php]
src   = "project-src"
entry = "index.php"

[web-server]
init = "init.php"     # PHP file, relative to [php] src; runs once at boot
```
<!-- @endcode-block -->

At build time `phpflash` turns a non-empty `[web-server] init` into a `-DPHP_WEB_INIT=<path>`
compile define (only for `web-server` projects — it is a no-op for the other project types). At boot
the firmware resolves that path against the same source mount the entry script lives on, checks the
file is readable, and runs it once inside the request that `php_embed_init()` already opened. If the
file is configured but missing, the firmware logs a warning and carries on without it.

<!-- @callout variant="warning" title="A failing init does not brick the device" -->
The init run is wrapped so that if the script throws or fatals, the failure is **logged and the
server still starts**. A broken init script must not leave a headless device unreachable, so the
HTTP server always comes up regardless. The init phase runs with `headers_sent` reset, so it may use
session and header operations just like a run-once script.
<!-- @endcallout -->

## The paired example: `web-init-mem`

The `web-init-mem` example puts both pieces together. `init.php` seeds the in-RAM store once at
boot; `index.php` reads what was seeded and keeps a per-request counter. Everything shared lives in
`mem_*`, below userland.

<!-- @code-block language="php" label="init.php — runs once at boot, output to the console" -->
```php
<?php
// server_init: runs once, before the HTTP server starts. Output goes to the serial console.
// Do one-time setup here whose effect lives below PHP and is shared by every request.
echo "[init] seeding mem_*\n";
mem_set('boot_msg', 'initialised once at boot');
mem_set('hits', 0);
```
<!-- @endcode-block -->

<!-- @code-block language="php" label="index.php — runs per request" -->
```php
<?php
// Each HTTP request is a fresh PHP cycle, so userland does not persist -- but mem_* does.
// Read the value init.php seeded and keep a request counter in RAM (no flash wear).
$hits = (int) mem_get('hits', 0) + 1;
mem_set('hits', $hits);

header('Content-Type: text/plain');
echo mem_get('boot_msg', '(unset)'), "\n";
echo "request #$hits since boot\n";
echo "mem keys: ", implode(', ', mem_keys()), "\n";
```
<!-- @endcode-block -->

At boot the console shows the init script run exactly once, then the server coming up:

<!-- @code-block language="text" label="Serial console at boot" -->
```
--- web-server init: /app/init.php ---
[init] seeding mem_*
--- web-server init done ---
web-server model: serving /app/index.php over HTTP on :80
```
<!-- @endcode-block -->

Then each request reads the once-seeded `boot_msg` and bumps the RAM counter:

<!-- @code-block language="text" label="Two requests" -->
```
$ curl http://<board-ip>/
initialised once at boot
request #1 since boot
mem keys: boot_msg, hits

$ curl http://<board-ip>/
initialised once at boot
request #2 since boot
mem keys: boot_msg, hits
```
<!-- @endcode-block -->

`boot_msg` was written once by `init.php` and is read by every request. `hits` climbs in RAM with no
flash cost. Reset the board and the count restarts from one — `mem_*` is volatile. For state that
must survive a reboot, use `store_*` instead (see [persistent-store.md](./persistent-store.md)).

## Handles versus data, one more time

Note that any **object or variable** created in `init.php` is gone once its run ends. The init phase
is a request cycle like any other; its local scope does not carry into the HTTP requests that
follow. What carries is only the effects it leaves **below** PHP:

<!-- @code-block language="php" label="Bringing a display up in init, sharing a value via mem_*" -->
```php
// init.php — runs once at boot
ssd1306_begin();                          // hardware handle -> lives in the C extension
mem_set('boot_msg', 'initialised once');  // data -> in-RAM store
```
<!-- @endcode-block -->

<!-- @code-block language="php" label="index.php — every request uses what init left behind" -->
```php
// index.php — every request
echo mem_get('boot_msg');                 // reads what init seeded (a fresh copy)
ssd1306_text('a request arrived');        // uses the already-initialised display
```
<!-- @endcode-block -->

The display handle lives in the C extension because a live resource cannot be serialized or kept
alive across requests; the message lives in `mem_*` because it is plain data. That is the rule of
thumb for the whole web-server model:

- **data** (scalars, arrays, serializable objects) → `mem_*` (volatile) or `store_*` (persistent);
- **live handles** (a display, a socket, a bus) → a C extension — `init.php` brings the handle up
  once and every request calls the extension's functions.

## Why not re-initialise on every request?

You could re-run hardware setup and recompute boot state at the top of every request, but it wastes
time on the per-request path and, for a display, can visibly flicker as it re-initialises. The
`init` script does that work once; `mem_*` hands the result forward cheaply. The one-time lifecycle —
hardware bring-up, boot configuration — is kept out of the request path where it does not belong,
and the request path is left doing only per-request work.
