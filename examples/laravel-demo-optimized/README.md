# laravel-demo-optimized

The [`laravel-demo`](../laravel-demo/) app, tuned to boot faster on the board. Same firmware, same
`web-server` mode — the difference is entirely in the app under `project-src/`.

## Where the time goes

Every HTTP request runs the framework from scratch: hundreds of PHP files opened off the (slow)
microSD and, without a bytecode cache, tokenized and compiled each time. Vanilla Laravel starts at
~20 s per request; this example gets it to **~8.4 s** by attacking both halves — the compile (with
OPcache) and everything around it (autoloader, I/O).

## What's tuned

- **OPcache** (`[extensions.opcache] enabled = true`) — the biggest single lever. The compiled
  bytecode is cached on the microSD, so after the first (warm-up) request each one skips the
  tokenize/parse/compile/optimize entirely. This alone takes the page from ~12 s to ~8.4 s. The
  cache stays on the card rather than in RAM because Laravel's bytecode plus its per-request heap
  don't both fit in the 32 MB PSRAM — see [`docs/opcache.md`](../../docs/opcache.md).

The rest are path-portable app-level tweaks (see "What's deliberately left out" for why that matters):

- **Authoritative classmap autoloader** — `composer install --no-dev --optimize-autoloader
  --classmap-authoritative`. This is the big lever on this board: the default PSR-4 autoloader does a
  filesystem probe (`file_exists` across each namespace prefix) for every class it loads — thousands
  of slow FATFS syscalls per request. An authoritative classmap turns each class load into one array
  lookup and one `require`, no probing.
- **No dev dependencies** — `--no-dev` drops `collision`, `pail`, `phpunit`, `faker`, `mockery`, so
  their service providers aren't discovered or booted.
- **No per-request disk I/O from framework services**, via `.env`:
  - `SESSION_DRIVER=array` — sessions live in memory (no session file read+write per request). They
    don't persist across requests, so CSRF-protected forms won't round-trip; fine for a read-only
    demo, switch to `file` if you need them.
  - `CACHE_STORE=array`, `QUEUE_CONNECTION=sync`, `BROADCAST_CONNECTION=null` — no cache/queue tables
    or files touched.
  - `LOG_CHANNEL=null` — logging is a no-op (the board has no useful log sink and every FATFS write
    is slow). Switch to a real channel to debug.
  - `APP_ENV=production`, `APP_DEBUG=false` — no debug error-page renderer or collectors.
- **Cached event map** — `php artisan event:cache` (`bootstrap/cache/events.php`), so listener
  discovery doesn't scan the app each request.
- **`Route::view('/', 'welcome')`** instead of a closure route (`routes/web.php`) — a small, tidy
  change (also keeps the route table serializable).

## What's deliberately left out

`php artisan optimize` normally also runs **config:cache**, **route:cache** and **view:cache**. Those
are **not** used here on purpose: they bake **absolute host paths** into the cached files
(`view.paths`, `view.compiled`, the sqlite path, the `/up` health route's template, the disk-serve
routes, …). This example is cross-compiled on a PC and then copied to the card, where the app lives
at `/sdcard` — so those baked PC paths would be wrong and break rendering. Blade's compiled-view
filenames are also hashed from the source's absolute path, so precompiled views wouldn't match
on-device anyway. Config, routes and views are therefore resolved at runtime (cheap next to the
framework compile), keeping the app portable with no hardcoded `/sdcard`.

## Reproducing it from a vanilla app

Starting from a stock `laravel/laravel` (as in `laravel-demo`):

```sh
cd project-src
# edit .env (see above) and routes/web.php (Route::view)
composer install --no-dev --optimize-autoloader --classmap-authoritative
php artisan event:cache
# do NOT run config:cache / route:cache / view:cache -- they bake PC paths (see above)
```

## Preparing the card and running

Copy the **contents** of `project-src/` to the **root of the microSD** (`/public/index.php`,
`/vendor/`, `/bootstrap/`, `/storage/` writable, `.env`, …), insert the card, reset. `storage/` and
`bootstrap/cache/` must stay writable (they are, on FAT). This build **adds OPcache** over
`laravel-demo`, so flash it:

```sh
phpflash build && phpflash flash && phpflash monitor
```

The firmware creates `/sdcard/opcache` for the bytecode cache. The **first** request after a fresh
card warms it (compiles + writes the cache) and is as slow as before; every request after that is
~8.4 s. OPcache doesn't check file mtimes here, so **after you change the app code, delete
`/sdcard/opcache`** (or the board keeps serving the old bytecode) — see
[`docs/opcache.md`](../../docs/opcache.md).

Then open `http://<board-ip>/` and compare the per-request time against `laravel-demo`. The welcome
page renders identically (`GET /` → `200`, the full welcome page); the gain is in how long the board
takes to produce it — verified on hardware at ~8.4 s.
