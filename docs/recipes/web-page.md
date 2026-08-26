---
eyebrow: 'Docs · Recipes'
lede:    'Serve an HTTP page straight from the board: run an HTTP server in front of PHP, hand each request to a fresh index.php with $_SERVER/$_GET/$_POST populated, and reach it at the board IP — from a hand-written page up to stock Laravel.'
see_also:
  - { href: '../getting-started/quick-start.md', meta: '10 min' }
  - { href: '../storage/in-ram-store.md', meta: '5 min' }
  - { href: '../extensions/porting-status.md', meta: '6 min' }
prev: { label: 'Read and write the microSD', href: './microsd-files.md' }
next: { label: 'Query SQLite with PDO', href: './sqlite-pdo.md' }
---

# Serve a web page

This recipe serves an HTTP page from the board itself. The firmware runs an HTTP server, and every request is handed to a fresh PHP run — the `web-server` execution model. You write the page; whatever you echo becomes the response body. The same model that serves a hand-written `index.php` also serves a stock Laravel app, so this page starts small and ends by pointing at the `laravel-demo` example.

## What you need

- A **networked** board. The `web-server` model needs a wired link, so pick a `-ETH` variant: **ESP32-P4-ETH** or **ESP32-S3-ETH**. A board with no network (a `-Pico` or `-Zero`) will not even offer `web-server` at `init` time.
- A USB cable that carries **data**, for flashing and the serial console.
- An Ethernet cable into the board's RJ45, on a network that hands out a DHCP address.

## The web-server model in one paragraph

Each HTTP request is a fresh PHP request cycle, shared-nothing — exactly like a script behind Apache or PHP-FPM. The firmware turns the incoming request into a CGI-style environment (`$_SERVER` with method and URI, plus `$_GET`, `$_POST`, `$_COOKIE`), runs your entry script, and the status, headers, cookies and body it produces become the HTTP response. When the request ends, the engine tears userland down: variables, objects and resources are gone before the next request starts. Static files under a `public/` root are served directly, without booting PHP. Because nothing you assign in one request survives into the next, state that must carry forward goes below userland — see the [in-RAM store](../storage/in-ram-store.md) for `mem_*` and the run-once `[web-server] init` script.

## A minimal example

Put this in `project-src/index.php`. It reads the request line out of `$_SERVER`, echoes an HTML page, and prints a fresh random number so you can see each request really re-executed rather than being cached.

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
// Runs fresh for every HTTP request, behind the firmware's HTTP server.
// You don't manage the socket or a loop: just produce the page, and
// whatever you echo becomes the response body.

$uri    = $_SERVER['REQUEST_URI']    ?? '/';
$method = $_SERVER['REQUEST_METHOD'] ?? 'GET';
$name   = $_GET['name'] ?? 'world';

echo "<!doctype html>\n";
echo "<html><head><meta charset=\"utf-8\"><title>PHP on ESP32</title></head>\n";
echo "<body style=\"font-family:sans-serif;max-width:34rem;margin:3rem auto\">\n";
echo "<h1>Hello, " . htmlspecialchars($name) . "</h1>\n";
echo "<p>Served fresh by PHP " . PHP_VERSION . " on an ESP32 microcontroller.</p>\n";
echo "<ul>\n";
echo "  <li>you requested: <code>" . htmlspecialchars("$method $uri") . "</code></li>\n";
echo "  <li>a fresh random number: " . random_int(1000, 9999) . "</li>\n";
echo "</ul>\n";
echo "</body></html>\n";
```
<!-- @endcode-block -->

Add `?name=esp32` to the URL and the heading changes — that is `$_GET` populated per request. Always run untrusted input through `htmlspecialchars()` before echoing it into the page.

<!-- @callout variant="note" title="No counter in a hand-written page" -->
Each request is a clean run, so there is no in-scope variable to count with. To keep a hit counter or any value across requests, write it to `mem_*` — the volatile in-RAM store that lives below userland. See the [in-RAM store](../storage/in-ram-store.md).
<!-- @endcallout -->

## Running a real framework

The interesting part is that the same model serves an unmodified framework. The `laravel-demo` example is a stock `composer create-project laravel/laravel` (Laravel 13) with nothing edited: the board runs its front controller (`public/index.php`) per request, so routing, sessions, cookies and 404s are all real. `GET /` returns the full welcome page with Laravel's own headers and session cookies; `GET /no-such-route` returns Laravel's real `404`; and static files under `public/` are served directly, without booting the framework.

Three things make that work:

<!-- @steps -->
- **`type = "web-server"`** and **`entry = "public/index.php"`** — the HTTP server in front, pointed at the framework's front controller instead of a bare `index.php`.
- **The framework's extension stack**, all built into the firmware: `mbstring` (with `onig` for native `mb_split`), `ctype`, `filter`, `tokenizer`, `session`, `openssl`, `sqlite` and `date`. See the [porting status](../extensions/porting-status.md).
- **OPcache**, enabled with `[extensions.opcache]`. Without it each request recompiles the whole framework, so a page takes on the order of 10–30 s; with the bytecode cache on the microSD it drops to a few seconds. The `laravel-demo-optimized` example turns it on.
<!-- @endsteps -->

The whole app — including `vendor/` (~30 MB) — is far too big for the embedded image, so it lives on the **microSD**: you copy the contents of `project-src/` to the card root yourself, and the firmware serves it from there. See the example's own README for the card-prep steps (`composer install`, `php artisan key:generate`).

<!-- @callout variant="warning" title="Frameworks need the P4's PSRAM" -->
A full framework leaves ~33 MB of PSRAM free on the ESP32-P4-ETH (up to 32 MB PSRAM) — plenty. The ESP32-S3-ETH has only 8 MB of PSRAM, which is tight: it runs the hand-written page and lighter apps fine, but a stock Laravel or Symfony can run out of memory mid-request. Use a P4 for the frameworks.
<!-- @endcallout -->

## Config

A minimal `web-server` project on the networked P4 board. For the hand-written page the entry is a bare `index.php`; for a framework, point `entry` at `public/index.php` and enable the framework's extensions plus `opcache`.

<!-- @code-block language="toml" label="php-esp32.config.toml — minimal web-server" -->
```toml
name = "web-page"
storage_type = "microsd"   # where the PHP source lives
type = "web-server"        # HTTP server in front, PHP per request

[board]
target = "esp32-p4-eth"    # a networked board -- web-server needs the wired link
port   = ""                # empty = autodetect at flash time

[esp-idf]
path    = ""
version = ""

[php-esp32]
path    = ""
version = ""

[php]
src   = "project-src"      # PHP source folder (copied to the microSD)
entry = "index.php"        # bare page; a framework sets public/index.php
```
<!-- @endcode-block -->

To seed data or bring hardware up once before the server starts, add a `[web-server] init` script — it runs a single time at boot with its output going to the serial console. See the [in-RAM store](../storage/in-ram-store.md) for the paired `init` + `mem_*` pattern.

## Build & flash

Build the firmware, write it to the board, then open the serial monitor.

<!-- @code-block language="bash" label="build, flash, monitor" -->
```bash
phpflash build && phpflash flash && phpflash monitor
```
<!-- @endcode-block -->

With `storage_type = "microsd"` the PHP source is read from the card at boot, so to change the page you copy `project-src/` to the card root and press reset — no rebuild. If OPcache has already cached the old code, clear the cache folder on the card too so the board recompiles.

## What you'll see

The board brings the Ethernet link up, gets a DHCP address, and prints it on the serial log. That line is where you point a browser or `curl`.

<!-- @code-block language="text" label="serial output (excerpt)" -->
```text
php-esp32: microSD mounted at /sdcard
network up -- http://192.168.1.42/
web-server model: serving /sdcard/index.php over HTTP on :80
```
<!-- @endcode-block -->

Open that address in a browser, or hit it with `curl`:

<!-- @code-block language="bash" label="terminal — request the board" -->
```bash
curl -i http://192.168.1.42/
curl "http://192.168.1.42/?name=esp32"
```
<!-- @endcode-block -->

`-i` shows the status line and headers; refresh a few times and the random number changes, confirming each request is a fresh PHP run.

## Next

For an app that stores data, continue to [Query SQLite with PDO](./sqlite-pdo.md). To keep a counter or a small cache across requests without touching flash, use the [in-RAM store](../storage/in-ram-store.md). To run a full framework end to end, copy the `laravel-demo` (or `laravel-demo-optimized`, with OPcache) example and follow its README.
