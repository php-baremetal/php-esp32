# symfony-demo

**Symfony on the ESP32-P4.** `project-src/` is a stock `symfony/skeleton` (Symfony **7.4**) plus one
controller, served over HTTP by the firmware's `web-server` model — one request at a time, browsable
at the board's IP.

> The whole app — including `vendor/` (~11 MB) — lives on the **microSD**, not in flash. You copy it
> to the card yourself.

## Why Symfony 7.4 (and PHP 8.3)

The target is PHP **8.3.32**. Symfony **8.x needs PHP 8.4**, so this uses the latest **7.x** (7.4,
which supports PHP 8.2+). Dependencies are pinned with `composer config platform.php 8.3.32`.

## The extension story

Symfony hard-requires three extensions at the composer level: `ext-ctype`, `ext-iconv`, `ext-xml`.

- **`ctype`** is ported (native here) — used.
- **`iconv`** and **`xml`** are **not** ported (no `libxml2` on this target, and no pure-PHP polyfill
  for XML). But a **minimal** Symfony page — attribute-routed, running from the pre-compiled prod
  container — doesn't actually *call* iconv or XML at runtime. That's verified: the page renders
  `200` on the desktop with both extensions' functions disabled. So the two requirements are
  satisfied with composer **platform overrides** (`composer config platform.ext-iconv 8.3.32`,
  `… platform.ext-xml 8.3.32`) — telling composer they're present so `install` succeeds — and the app
  simply never reaches them.

  The catch: this only holds for an app that doesn't use XML/DOM (config in PHP/YAML + attributes, no
  `DomCrawler`/`Serializer` XML, no XML translations/validation) or iconv. If you add code that does,
  you'll need `symfony/polyfill-iconv` for iconv, and — for XML — a `libxml2` port (not done yet).

Everything else it needs is ported: `mbstring`, `filter`, `tokenizer`, `session`, `date`, plus the
always-on built-ins (`pcre`, `spl`, `reflection`, `hash`, `json`, `random`).

## The prod cache

Symfony compiles its service container (and caches routes) into `var/cache/prod/`. **This example
ships without a pre-built cache on purpose**: a warmed cache bakes the *absolute* paths of wherever it
was built, which would be wrong on the card (`/sdcard`). Instead the board compiles the container on
the **first** request — writing to `var/cache/` on the card (FAT is writable) with the right paths —
so `var/` must stay writable. That first request is slow (like any cold Symfony boot); every one after
is fast, and OPcache (enabled here) keeps the generated container compiled.

## Preparing the card

1. On your PC (deps are already pinned to PHP 8.3 in `composer.json`):
   ```sh
   cd project-src
   composer install --no-dev --optimize-autoloader   # platform is pinned; xml/iconv bypassed
   ```
2. Copy the **contents** of `project-src/` to the **root of the microSD**:
   ```
   /public/index.php   /src/   /vendor/   /config/   /var/ (writable)   .env   .env.local   ...
   ```
   `var/` must be writable (it is, on FAT) — the board writes the compiled container there.
3. Insert the card and reset.

## Building, flashing, browsing

```sh
phpflash build && phpflash flash && phpflash monitor
```

Watch for `network up -- http://<ip>/`, then open that address. The **first** request compiles the
container (slow); reload and it's quick. If you change the app code, delete `var/cache/` **and**
`/sdcard/opcache` on the card so the board rebuilds both.

## Status

Verified **on the ESP32-P4**: `GET /` → `200`, the welcome page, running the pre-compiled prod
container from the microSD. Steady-state is **~2.1 s per request** (OPcache file-cache warm; the first
request after a cold `var/cache/` is slower, as it compiles the container). `iconv`/`xml` are bypassed
at the composer level and never reached at runtime — this is the minimal, XML-free slice of Symfony
that fits the ported extensions.

Getting there needed a handful of generic FATFS/POSIX fixes in the firmware (mount-root `stat`, a
`readdir`-based `glob`, `rename`-overwrite, and presenting a `cli-server` SAPI name) — all in
[`docs/porting-notes.md`](../../docs/porting-notes.md), and all shared with the other web-server
examples.
