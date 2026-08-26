# web-init-mem

Sharing setup and data across requests in the **web-server** model, with the two pieces built for
it: a one-time `server_init` script and the in-RAM `mem_*` store. See [docs/storage/in-ram-store.md](../../docs/storage/in-ram-store.md).

## What it shows

In the web-server model each HTTP request is a fresh PHP cycle -- userland does not survive between
requests. So one-time setup and any shared data must live below PHP.

`init.php` runs **once**, at boot, before the HTTP server starts (its output goes to the console):

```php
// init.php
mem_set('boot_msg', 'initialised once at boot');
mem_set('hits', 0);
```

`index.php` runs **per request**, reads what init seeded, and keeps a request counter in RAM:

```php
// index.php
$hits = (int) mem_get('hits', 0) + 1;
mem_set('hits', $hits);
echo mem_get('boot_msg'), "\nrequest #$hits since boot\n";
```

The init script is wired up in `php-esp32.config.toml`:

```toml
type = "web-server"

[web-server]
init = "init.php"     # relative to [php] src; runs once at boot
```

## Running it

The web-server model needs a networked board (here `esp32-p4-eth`). At boot the console shows the
init script run once:

```
--- web-server init: /app/init.php ---
[init] seeding mem_*
--- web-server init done ---
web-server model: serving /app/index.php over HTTP on :80
```

Then each request bumps the RAM counter (the board's IP is printed at boot):

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

`boot_msg` was written once by `init.php` and read by every request; `hits` climbs in RAM without
touching flash. Reset the board and the count starts over -- `mem_*` is volatile (for state that must
survive a reboot, use `store_*` instead; see [docs/storage/persistent-store.md](../../docs/storage/persistent-store.md)).

## The rule of thumb

- **data** (scalars, arrays, serializable objects) -> `mem_*` (volatile) or `store_*` (persistent);
- **live handles** (a display, a socket) -> a C extension; a resource cannot be shared across
  requests, so `init.php` just brings it up and requests call the extension's functions.
