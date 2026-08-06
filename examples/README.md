# Examples

Almost every example is a self-contained phpflash project: a `php-esp32.config.toml` that selects the
board, the storage mode and any optional extensions, plus a `project-src/` folder holding the PHP
code. (The two Eloquent examples are the exception, see the note below.) To try one, `cd` into its
folder and:

```sh
phpflash build      # build the firmware, with the extensions the config declares
phpflash flash      # flash it to the board
phpflash monitor    # watch the serial output
```

The configs target the ESP32-P4-Pico by default (the P4-ETH for the networked ones). To build for a
different board, change `[board] target` in the config, or pass `--board` when you scaffold your own
project. The `web-server` examples work on any networked board (P4-ETH or S3-ETH); the framework
examples (Laravel, Symfony) need the P4's larger PSRAM.

To run from a microSD instead of flashing the source in, copy the contents of `project-src/` to the
card root (so its `index.php` sits at `/index.php`), put the card in the board and press reset. The
script runs on its own and the output comes out on the serial port.

## What a folder looks like

Every example holds the same things:

- `php-esp32.config.toml`: the project config (board, storage mode, and which optional extensions to
  compile in). This is what tells phpflash how to build the firmware, so you never pass build flags by
  hand.
- `project-src/`: the PHP source (`index.php`, and any extra files or a Composer `vendor/`). This is
  the deployable: what a `phpflash build` packs in, or what you copy to a microSD.
- `README.md`: what the example does and demonstrates, with an excerpt of the output.
- `monitor.txt`: the serial log of a real run on an ESP32-P4, so you can see the result without a
  board in front of you.

Many examples also include an image of the result: a `display.gif` clip for the hardware ones (the LED
blinking, the button turning it on) and an `output.png` screenshot for the web-server ones (the page
in a browser).

## The examples

| Folder | What it does | Needs |
|---|---|---|
| [`hello/`](hello/) | A linear script: prints and stops. The bare minimum to see the engine alive. | nothing |
| [`language-tour/`](language-tour/) | A tour through closures, generators, classes, `match`, exceptions and the standard library, timing each step. Proof it is real PHP. | nothing |
| [`require-demo/`](require-demo/) | A program split across several files with `require` and `require_once`. | nothing |
| [`composer-collections/`](composer-collections/) | Composer autoloading with the Illuminate Collections package. | `vendor/` |
| [`led-blink/`](led-blink/) | The setup/loop model: blinks an LED forever. | LED + ~330 ohm between GPIO2 and GND |
| [`blink-sos/`](blink-sos/) | Blinks "SOS" in Morse code on the LED. | same as above |
| [`button-led/`](button-led/) | Reads a push button and mirrors it to the LED. | LED on GPIO2, button between GPIO4 and GND |
| [`sd-write/`](sd-write/) | Writes a file to the microSD and reads it back, a quick check of the write path. | a microSD |
| [`sqlite-notes/`](sqlite-notes/) | PDO opens a SQLite database on the microSD and writes a row each boot. | `sqlite` extension |
| [`date-timezones/`](date-timezones/) | `DateTime` across named timezones, DST-aware conversions and interval math. | `date` extension (full tz db) |
| [`date-utc/`](date-utc/) | `DateTime` in a UTC-only build: what still works and the named zones you give up. | `date` extension (UTC-only db) |
| [`ctype-demo/`](ctype-demo/) | The `ctype_*` character-class checks on whole strings. | `ctype` extension |
| [`mbstring-demo/`](mbstring-demo/) | Multibyte strings: `mb_strlen`, `mb_substr`, case, encoding detect and convert. | `mbstring` extension |
| [`mbstring-no-cjk/`](mbstring-no-cjk/) | The same with the CJK encodings dropped (~755 KB smaller mbstring). | `mbstring` + `no_cjk` |
| [`mbstring-regex/`](mbstring-regex/) | `mb_ereg` and `mb_split` with Unicode patterns, on the real oniguruma engine. | `mbstring` + `onig` |
| [`filter-demo/`](filter-demo/) | `filter_var()` validation and sanitization (email, int, URL, IP). | `filter` extension |
| [`tokenizer-demo/`](tokenizer-demo/) | `token_get_all()` and `PhpToken` breaking PHP source into tokens with the engine's lexer. | `tokenizer` extension |
| [`session-demo/`](session-demo/) | `session_start()` and `$_SESSION` persisting to the microSD, surviving reboots. | `session` extension + a microSD |
| [`openssl-compat/`](openssl-compat/) | AES encryption via the mbedTLS-backed openssl subset (symmetric only, ~42 KB). | `openssl` extension |
| [`openssl-full/`](openssl-full/) | Real OpenSSL: RSA sign and verify, encrypt, full digests, on-chip key generation (~2 MB). | `openssl` + `full` |
| [`https-client/`](https-client/) | A certificate-verified HTTPS GET from PHP over the esp-tls TLS client. | networked board, `openssl` + `full` + `tls` |
| [`eloquent-demo/`](eloquent-demo/) | Laravel's Eloquent ORM, standalone, on a SQLite database on the microSD, with mbstring without oniguruma (`mb_split` polyfilled). | "everything" firmware + `vendor/` |
| [`eloquent-onig/`](eloquent-onig/) | The same Eloquent demo on a firmware with mbstring built with oniguruma (native `mb_split`). | "everything" firmware + `onig` + `vendor/` |
| [`laravel-demo/`](laravel-demo/) | Vanilla Laravel (unmodified `laravel/laravel`) on the microSD, browsable over HTTP via the `web-server` mode: real routing, sessions, and static files from `public/`. | P4-ETH + network, Laravel's ext stack + a big microSD |
| [`laravel-demo-optimized/`](laravel-demo-optimized/) | The same Laravel app tuned to boot faster (~20 s to ~8.4 s per request): OPcache bytecode cache, authoritative autoloader, no dev deps, no-I/O drivers. | as `laravel-demo` + `opcache` |
| [`symfony-demo/`](symfony-demo/) | Symfony 7.4 (skeleton + a controller) on the microSD, browsable over HTTP at ~2.1 s per request (prod, OPcache warm). A minimal, XML-free slice: Symfony's `ext-iconv` and `ext-xml` requirements are bypassed. | P4-ETH + network, `ctype`/`mbstring`/`tokenizer`/`session`/`date`/`opcache` + microSD |
| [`web-server-init-loop/`](web-server-init-loop/) | Serves a web page over Ethernet, with the whole HTTP server written in PHP (setup/loop). | networked board |
| [`web-server/`](web-server/) | The same page, using the firmware's `web-server` project type: a C HTTP server in front, PHP run fresh per request. | networked board |

The linear examples run once and finish; the hardware ones (`led-blink`, `blink-sos`, `button-led`)
use the setup/loop model and keep going as long as the board is powered. The `web-server` examples
need a networked board with its RJ45 plugged into a network; the firmware brings the link up at boot
and logs the address.

## The examples that need an extension

Some examples use optional native extensions that are off by default, since native code cannot be
side-loaded from the card and has to be compiled in. You do not pass any build flags: each example's
`php-esp32.config.toml` already enables what it needs, and `phpflash build` reads it. The
`[extensions.*]` block in each config:

- [`sqlite-notes`](sqlite-notes/): `[extensions.sqlite]`
- [`date-timezones`](date-timezones/): `[extensions.date]` (full timezone database)
- [`date-utc`](date-utc/): `[extensions.date]` with `minimal_tz = true` (UTC-only, smaller)
- [`ctype-demo`](ctype-demo/): `[extensions.ctype]`
- [`mbstring-demo`](mbstring-demo/): `[extensions.mbstring]`
- [`mbstring-no-cjk`](mbstring-no-cjk/): `[extensions.mbstring]` with `no_cjk = true`
- [`mbstring-regex`](mbstring-regex/): `[extensions.mbstring]` with `onig = true`
- [`filter-demo`](filter-demo/): `[extensions.filter]`
- [`tokenizer-demo`](tokenizer-demo/): `[extensions.tokenizer]`
- [`session-demo`](session-demo/): `[extensions.session]`
- [`openssl-compat`](openssl-compat/): `[extensions.openssl]` (mbedTLS subset)
- [`openssl-full`](openssl-full/): `[extensions.openssl]` with `full = true` (real OpenSSL, ~2 MB)

If an extension pulls in a library (SQLite's amalgamation, mbstring's oniguruma), `phpflash build`
runs the fetch step for you the first time.

## The two Eloquent examples

[`eloquent-demo`](eloquent-demo/) and [`eloquent-onig`](eloquent-onig/) use the whole extension set at
once: `sqlite`, `mbstring`, `ctype`, `filter` and `date` (plus `onig` for `eloquent-onig`). They are
kept in the older single-file layout (an `index.php` and a `vendor/` at the folder root, not a
phpflash project): their `vendor/` is around 13 MB, larger than the embedded flash image, so they are
built and deployed by hand and their `monitor.txt` is from an earlier microSD run. Each one's README
walks through the manual build.

## The examples that need more than one file

Most examples are just an `index.php` inside `project-src/`. A few carry more:

- [`require-demo`](require-demo/) is deliberately made of several files (`index.php`, `config.php`,
  `lib/shapes.php`) under `project-src/`.
- [`composer-collections`](composer-collections/), [`eloquent-demo`](eloquent-demo/) and
  [`eloquent-onig`](eloquent-onig/) need a `vendor/` tree next to `index.php`. `vendor/` is not
  committed: generate it with `composer install` inside `project-src/` (the firmware needs FAT long
  filenames, already enabled in the default `sdkconfig`). Their READMEs walk through it.
