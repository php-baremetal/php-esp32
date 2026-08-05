# laravel-demo

**Vanilla Laravel — unmodified — on the ESP32-P4.** `project-src/` is a stock
`composer create-project laravel/laravel` (Laravel 13), nothing edited. The board runs its front
controller (`public/index.php`) behind an HTTP server (the `web-server` project type): every request
runs Laravel fresh, so it's a real browsable app — routing, sessions, cookies and all — reachable at
the board's IP.

> The whole app — including `vendor/` (~30 MB) — lives on the **microSD**, not in flash. You copy it
> to the card yourself; it's far too big for the embedded image.

## Why it works now

Laravel 13's framework requires exactly these PHP extensions — and this firmware has **all** of them:

`ctype` · `filter` · `hash` · `mbstring` · `openssl` · **`session`** · **`tokenizer`**

(`session` and `tokenizer` were the last pieces; see [`docs/ext-porting.md`](../../docs/ext-porting.md).)
The example also builds `sqlite` (Laravel's default database + database session/cache drivers),
`date` (Carbon), and `mbstring` **with oniguruma** (`onig`) so `mb_split` is native — Laravel calls
it and there's no polyfill in a vanilla app.

Deps are locked to the target's PHP with `composer config platform.php 8.3.32` — the only setup step,
and it touches no Laravel code.

## Preparing the card

1. Build the app (on your PC, with PHP 8.3-compatible deps):
   ```sh
   cd project-src
   composer install          # platform is pinned to php 8.3.32 in composer.json
   php artisan key:generate   # if .env has no APP_KEY yet
   ```
2. Copy the **contents** of `project-src/` to the **root of the microSD**, so the card has:
   ```
   /public/index.php   /vendor/   /bootstrap/   /app/   /storage/   /database/   .env   ...
   ```
   `storage/` and `bootstrap/cache/` must be writable (they are, on FAT).
3. Insert the card and reset the board.

## Building and flashing the firmware

```sh
phpflash build && phpflash flash && phpflash monitor
```

The firmware bundles Laravel's whole extension stack (~6 MB image) and points the entry at
`public/index.php` (`[php] entry` → `-DPHP_ENTRY`). The `web-server` project type puts an HTTP server
in front: each request is turned into a full CGI-style `$_SERVER` / `$_GET` / `$_POST` / `$_COOKIE`,
Laravel runs, and the headers, status, cookies and body it produces become the HTTP response.

## Browsing it

Once the board has an IP (shown on the serial log — `network up -- http://<ip>/`), open that address
in a browser, or:

```sh
curl -i http://<board-ip>/
```

Each request recompiles the framework (there's no opcache), so a page takes on the order of 10–30 s.

## Status — it works

**Vanilla Laravel 13 runs as a browsable web app on the ESP32-P4**, verified on hardware over HTTP:

- `GET /` → `200 OK`, the full ~70 KB welcome page, with Laravel's own headers
  (`X-Powered-By: PHP/8.3.32`, `Cache-Control`) and the session cookies it sets
  (`Set-Cookie: XSRF-TOKEN=…`, `Set-Cookie: laravel-session=…`).
- `GET /no-such-route` → `404 Not Found`, Laravel's Not Found page — real routing and real status
  codes, not a canned response.
- `GET /robots.txt` → the raw file from `public/`, `text/plain`, in ~5 ms — static files under
  `public/` are served directly, without booting the framework (like `try_files $uri /index.php`).
  Routes still fall through to Laravel; `.php` files are never served as static.

It leaves ~33 MB of PSRAM free.

Getting stock Laravel to run took three **firmware** fixes for gaps in the ESP-IDF FATFS filesystem
(all general, in `main/fs_pathnorm.c`; no change to Laravel itself):

- **`.` / `..` in paths** — FATFS doesn't resolve them, but Laravel builds
  `__DIR__."/../../../../config"`. The syscalls are `--wrap`ped to normalize paths first.
- **`stat()` on directories** — worked, but only once path normalization was in place.
- **`lstat()` / `realpath()`** — FATFS's `lstat` is unimplemented, which broke PHP's `realpath()`
  (it `lstat`s every component); since FAT has no symlinks, `lstat` is routed to `stat`.
