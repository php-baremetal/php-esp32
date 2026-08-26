---
eyebrow: 'Docs · Recipes'
lede: 'Keep state between reboots and hand data from one web request to the next — a power-off-surviving boot counter in the NVS store, and a per-request hit counter in the volatile in-RAM table seeded once at boot.'
see_also:
  - href: ../storage/persistent-store.md
    meta: 'Storage & state'
    label: 'Persistent store (store_*)'
  - href: ../storage/in-ram-store.md
    meta: 'Storage & state'
    label: 'In-RAM store (mem_*)'
  - href: ../getting-started/architecture.md
    meta: 'Getting started'
    label: 'Execution models'
prev:
  label: 'Make an HTTPS request'
  href: './https-request.md'
next:
  label: 'Footprint'
  href: '../reference/footprint.md'
---

# Persist state

State does not survive on its own. In the run-once and loop models, userland variables vanish at reset; in the web-server model they vanish at the end of every HTTP request. This recipe covers the two tools php-esp32 gives you to keep data around: `store_*`, which survives a power-off, and `mem_*`, which survives from one request to the next within a boot.

## Goal

Keep a value across resets, and hand a value from one web request to the next. Two counters make the difference visible: a boot counter that climbs every time you reset the board, and a hit counter that climbs on every HTTP request but resets to zero when the board reboots.

## Two tools

The two stores answer two different questions. Pick by whether the state must outlive a reboot, and how often you write it.

<!-- @tabs labels="Across reboots (store), Across requests (mem)" -->
<!-- @tab index="0" -->

`store_*` is a key-value store backed by the SoC's **NVS** flash. A value written in one boot is read back in the next, so it survives a power-off. It costs a slice of flash — enable it with `[store] size_kb` — and because every write erases a flash cell, it is meant for state that changes **slowly**: a boot counter, a provisioning flag, a calibration constant. Values are strings; cast numbers on the way in and out.

<!-- @endtab -->
<!-- @tab index="1" -->

`mem_*` is a volatile in-RAM key-value table. It touches **no flash** and is **wiped on reboot**, but a value written in one HTTP request is still there in the next — the gap the shared-nothing web-server model otherwise leaves. It is always built in, needs no configuration, and is free to write on **every** request: a hit counter, a small cache, a rate-limit bucket. A companion `[web-server] init` script runs once at boot to seed it before the server starts.

<!-- @endtab -->
<!-- @endtabs -->

<!-- @callout variant="warning" title="store_* for slow writes, mem_* for hot writes" -->
NVS is wear-levelled, but flash cells still wear out per erase/write cycle. Do **not** `store_set` on every loop tick or HTTP request — a hot write loop will eventually exhaust the partition. Reserve `store_*` for values that change slowly and must persist; use `mem_*` for state that changes on every request and only needs to last until the next reboot.
<!-- @endcallout -->

## The code: a boot counter (store)

The [`store-demo`](https://github.com/php-baremetal/php-esp32/tree/master/examples/store-demo) example runs in the loop model. `setup()` reads the previous count (a string), bumps it, and writes it straight back — persisted the instant `store_set` returns. It writes a one-time message on the first boot and lists the stored keys, then guards the whole thing on `store_available()` so the same script still runs on a build without persistence configured.

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
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
    echo "  reset the board -- the count keeps climbing\n";
    echo "=============================================\n";
}

function loop(int $tick): void
{
    delay(5000);
}
```
<!-- @endcode-block -->

## The code: a hit counter (mem)

The [`web-init-mem`](https://github.com/php-baremetal/php-esp32/tree/master/examples/web-init-mem) example runs in the web-server model. `init.php` runs once at boot, before the server accepts connections, and seeds the in-RAM table. `index.php` runs per request: it reads what init seeded, bumps a counter in RAM, and returns both over HTTP.

<!-- @code-block language="php" label="project-src/init.php — runs once at boot, output to the console" -->
```php
<?php
// server_init: runs once, before the HTTP server starts. Output goes to the serial console.
// Seed the in-RAM mem_* store; every request reads what we leave here.
echo "[init] seeding mem_*\n";
mem_set('boot_msg', 'initialised once at boot');
mem_set('hits', 0);
```
<!-- @endcode-block -->

<!-- @code-block language="php" label="project-src/index.php — runs per request" -->
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

<!-- @callout variant="note" title="Anything in init.php's scope is gone when it ends" -->
The init phase is a request cycle like any other — its local variables do not carry into the HTTP requests that follow. What carries is only what it leaves **below** PHP: a value seeded in `mem_*`, or a live handle (a display, a socket) brought up in a C extension. Put data in `mem_*`; put live resources in an extension and let init initialise them once.
<!-- @endcallout -->

## Config for each

The boot counter needs a flash partition; the hit counter needs the web-server model with an init script named.

<!-- @tabs labels="Across reboots (store), Across requests (mem)" -->
<!-- @tab index="0" -->

`store_*` is **off by default** because it costs flash. Give it a size and `phpflash` adds a dedicated `phpstore` NVS partition to the generated table. A size below 16 KB is bumped up to that floor (NVS needs the headroom); 32 KB holds a few hundred small entries.

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
type = "init-loop"

[php]
src   = "project-src"
entry = "index.php"

# Reboot-persistent key-value store (store_set/store_get, NVS-backed). Give it 32 KB of flash.
[store]
size_kb = 32     # 0 or absent = no persistence
```
<!-- @endcode-block -->

<!-- @endtab -->
<!-- @tab index="1" -->

`mem_*` needs no configuration — it is always built in. What this side needs is the **web-server** model plus a `[web-server] init` script (a path relative to `[php] src`) so setup runs once before the first request. The web-server model needs a networked board.

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
type = "web-server"

[php]
src   = "project-src"
entry = "index.php"

[web-server]
init = "init.php"     # relative to [php] src; runs once at boot
```
<!-- @endcode-block -->

<!-- @endtab -->
<!-- @endtabs -->

## Build & flash

Same for either project:

<!-- @code-block language="bash" label="Build, flash, and watch the serial log" -->
```bash
phpflash build && phpflash flash && phpflash monitor
```
<!-- @endcode-block -->

## What you'll see

The boot counter climbs on every reset — the value lives in flash, not RAM. The serial monitor keeps running across resets, so you can watch it go up:

<!-- @code-block language="text" label="store-demo: serial output across four boots" -->
```
=== store-demo :: persistent boot counter ===
  boot count : 4   (survives resets)
  first_msg  : hello from boot #1
  keys       : first_msg, boots
  reset the board -- the count keeps climbing
=============================================
```
<!-- @endcode-block -->

The hit counter climbs on every HTTP request instead. At boot the console shows the init script run exactly once, then the server coming up:

<!-- @code-block language="text" label="web-init-mem: serial console at boot" -->
```
--- web-server init: /app/init.php ---
[init] seeding mem_*
--- web-server init done ---
web-server model: serving /app/index.php over HTTP on :80
```
<!-- @endcode-block -->

Then each request reads the once-seeded `boot_msg` and bumps the RAM counter:

<!-- @code-block language="bash" label="web-init-mem: three requests" -->
```bash
$ curl http://<board-ip>/
initialised once at boot
request #1 since boot
mem keys: boot_msg, hits

$ curl http://<board-ip>/
initialised once at boot
request #2 since boot
mem keys: boot_msg, hits

$ curl http://<board-ip>/
initialised once at boot
request #3 since boot
mem keys: boot_msg, hits
```
<!-- @endcode-block -->

`boot_msg` is written once by `init.php` and read by every request; `hits` climbs in RAM with no flash cost. The key difference is what a reset does: the `store_*` boot counter keeps climbing, while the `mem_*` hit counter starts over at #1 — `mem_*` is volatile. For state that must survive a reboot, use `store_*`.

The full examples live in [`examples/store-demo/`](https://github.com/php-baremetal/php-esp32/tree/master/examples/store-demo) and [`examples/web-init-mem/`](https://github.com/php-baremetal/php-esp32/tree/master/examples/web-init-mem).
