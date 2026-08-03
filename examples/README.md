# Examples

Almost every example is a self-contained **phpflash project**: a `php-esp32.config.toml` that
selects the board, the storage mode and any optional extensions, plus a `project-src/` folder
holding the PHP code. (The two Eloquent examples are the exception — see the note below.) To try
one, `cd` into its folder and:

```sh
phpflash build      # build the firmware (with the extensions the config declares)
phpflash flash      # flash it to the board
phpflash monitor    # watch the serial output
```

To run from a microSD instead of flashing the source in, copy the contents of `project-src/`
to the card root (so its `index.php` sits at `/index.php`), put the card in the board and press
reset — the script runs on its own and the output comes out on the serial port.

## What a folder looks like

Every example holds the same things:

- `php-esp32.config.toml` — the project config: board, storage mode, and which optional
  extensions to compile in. This is what tells `phpflash` how to build the firmware, so you
  never pass build flags by hand.
- `project-src/` — the PHP source (`index.php`, and any extra files or a Composer `vendor/`).
  This is the deployable: what a `phpflash build` packs in, or what you copy to a microSD.
- `README.md` — what the example does and what it demonstrates, with an excerpt of the output.
- `monitor.txt` — the serial log of a real run on an ESP32-P4-Pico, so you can see the result
  without a board in front of you.

The examples that use hardware also include a `display.gif`: a short clip of the circuit
working (the LED blinking, the button turning it on, and so on).

## The examples

| Folder | What it does | Needs |
|---|---|---|
| [`hello/`](hello/) | A linear script: prints and stops. The bare minimum to see the engine alive. | — |
| [`language-tour/`](language-tour/) | A tour through closures, generators, classes, `match`, exceptions and the standard library, timing each step: proof that it's real PHP. | — |
| [`require-demo/`](require-demo/) | A program split across several files with `require`/`require_once`. | — |
| [`composer-collections/`](composer-collections/) | Composer autoloading with the Illuminate Collections package. | `vendor/` |
| [`led-blink/`](led-blink/) | The `setup()`/`loop()` model: blinks an LED forever. | LED + ~330 Ω between GPIO2 and GND |
| [`blink-sos/`](blink-sos/) | Blinks "SOS" in Morse code on the LED. | same as above |
| [`button-led/`](button-led/) | Reads a push button and mirrors it to the LED. | LED on GPIO2, button between GPIO4 and GND |
| [`sd-write/`](sd-write/) | Writes a file to the microSD and reads it back — a quick check that the write path works. | a microSD |
| [`sqlite-notes/`](sqlite-notes/) | PDO opens a SQLite database on the microSD and writes a row each boot. | `sqlite` extension |
| [`date-timezones/`](date-timezones/) | `DateTime` across named timezones, DST-aware conversions and interval math. | `date` extension (full tz db) |
| [`date-utc/`](date-utc/) | `DateTime` in a UTC-only build: what still works and the named zones you give up. | `date` extension (UTC-only db) |
| [`ctype-demo/`](ctype-demo/) | The `ctype_*` character-class checks on whole strings. | `ctype` extension |
| [`mbstring-demo/`](mbstring-demo/) | Multibyte strings: `mb_strlen`, `mb_substr`, case, encoding detect/convert. | `mbstring` extension |
| [`mbstring-no-cjk/`](mbstring-no-cjk/) | The same, with the CJK encodings dropped (~755 KB smaller mbstring). | `mbstring` + `no_cjk` |
| [`mbstring-regex/`](mbstring-regex/) | `mb_ereg*` / `mb_split` with Unicode patterns, on the real Oniguruma engine. | `mbstring` + `onig` |
| [`filter-demo/`](filter-demo/) | `filter_var()` validation and sanitization (email, int, URL, IP…). | `filter` extension |
| [`eloquent-demo/`](eloquent-demo/) | Laravel's Eloquent ORM (standalone, no framework) on a SQLite database on the microSD — mbstring **without** oniguruma (`mb_split` polyfilled). | "everything" firmware + `vendor/` |
| [`eloquent-onig/`](eloquent-onig/) | The same Eloquent demo, on a firmware with mbstring built **with** oniguruma (native `mb_split`, no polyfill). | "everything" firmware + `onig` + `vendor/` |

The linear examples run once and finish; the hardware ones (`led-blink`, `blink-sos`,
`button-led`) use the `setup()`/`loop()` model and keep going as long as the board is powered.

## The examples that need an extension

Some examples use optional native extensions that are off by default (native code can't be
side-loaded from the card, so it has to be compiled in). You don't pass any build flags: each
example's `php-esp32.config.toml` already enables what it needs, and `phpflash build` reads it.
The `[extensions.*]` block in each config:

- [`sqlite-notes`](sqlite-notes/) — `[extensions.sqlite]`
- [`date-timezones`](date-timezones/) — `[extensions.date]` (full timezone database)
- [`date-utc`](date-utc/) — `[extensions.date]` with `minimal_tz = true` (UTC-only, smaller)
- [`ctype-demo`](ctype-demo/) — `[extensions.ctype]`
- [`mbstring-demo`](mbstring-demo/) — `[extensions.mbstring]`
- [`mbstring-no-cjk`](mbstring-no-cjk/) — `[extensions.mbstring]` with `no_cjk = true`
- [`mbstring-regex`](mbstring-regex/) — `[extensions.mbstring]` with `onig = true`
- [`filter-demo`](filter-demo/) — `[extensions.filter]`

If an extension pulls in a library (sqlite's amalgamation, mbstring's oniguruma), `phpflash
build` runs the fetch step for you the first time.

## The two Eloquent examples

[`eloquent-demo`](eloquent-demo/) and [`eloquent-onig`](eloquent-onig/) use the whole extension
set at once — `sqlite` + `mbstring` + `ctype` + `filter` + `date` (with `onig` for
`eloquent-onig`). They are kept in the **older single-file layout** (an `index.php` and a
`vendor/` at the folder root, not a phpflash project): their `vendor/` is ~13 MB, larger than the
embedded flash image, so they are built and deployed by hand and their `monitor.txt` is from an
earlier microSD run. Each one's README walks through the manual build. Everything else here is a
phpflash project.

## The examples that need more than one file

Most examples are just an `index.php` inside `project-src/`. A few carry more:

- [`require-demo`](require-demo/) is deliberately made of several files (`index.php`,
  `config.php`, `lib/shapes.php`) under `project-src/`.
- [`composer-collections`](composer-collections/), [`eloquent-demo`](eloquent-demo/) and
  [`eloquent-onig`](eloquent-onig/) need a `vendor/` tree next to `index.php`. `vendor/` is not
  committed: generate it with `composer install` inside `project-src/` (the firmware needs FAT
  long filenames, already enabled in the default `sdkconfig`). Their READMEs walk through it.
