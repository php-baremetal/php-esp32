# laravel-demo-optimized

The [`laravel-demo`](../laravel-demo/) app, tuned to boot faster on the board. Same firmware, same
`web-server` mode — the difference is entirely in the app under `project-src/`.

## Why it's slow to begin with

There is **no opcache** on this port, so every HTTP request recompiles the framework from source:
hundreds of PHP files tokenized and compiled each time. That, plus the microSD/FATFS filesystem
being slow at `open()`/`stat()`, is where the ~20 s per request goes. Opcache would be the real fix,
but it isn't ported — so this example attacks everything *around* the compile instead.

## What's tuned

Path-portable optimizations only — see "What's deliberately left out" for why that matters here.

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

Same as `laravel-demo`: copy the **contents** of `project-src/` to the **root of the microSD**
(`/public/index.php`, `/vendor/`, `/bootstrap/`, `/storage/` writable, `.env`, …), insert the card,
reset. `storage/` and `bootstrap/cache/` must stay writable (they are, on FAT).

The firmware is byte-for-byte the same as `laravel-demo`, so **if that firmware is already flashed you
don't need to reflash** — just swap the card contents. Otherwise:

```sh
phpflash build && phpflash flash && phpflash monitor
```

Then open `http://<board-ip>/` and compare the per-request time against `laravel-demo`. The welcome
page still renders identically (verified on desktop: `GET /` → `200`, the full welcome page); the
gain is in how long the board takes to produce it.
