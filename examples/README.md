# Examples

Each example lives in its own folder. To try one, copy its `index.php` to the microSD (in
the root, named `index.php`), put the card back in the board and press reset: the script
runs on its own and the output comes out on the serial port.

## What a folder looks like

Every example holds the same files:

- `index.php` — the example's PHP code, the only thing that actually needs to be on the card.
- `README.md` — what the example does and what it demonstrates, with an excerpt of the output.
- `monitor.txt` — the full serial log of that run, so you can see the real result without
  having to mount the card yourself.

The examples that use hardware also include a `display.gif`: a short clip of the circuit
working (the LED blinking, the button turning it on, and so on).

## The examples

| Folder | What it does | Hardware |
|---|---|---|
| [`hello/`](hello/) | A linear script: prints and stops. The bare minimum to see the engine alive. | none |
| [`language-tour/`](language-tour/) | A tour through closures, generators, classes, `match`, exceptions and the standard library, timing each step: proof that it's real PHP. | none |
| [`require-demo/`](require-demo/) | A program split across several files with `require`/`require_once`. | none |
| [`composer-collections/`](composer-collections/) | Composer autoloading with the Illuminate Collections package. | none |
| [`led-blink/`](led-blink/) | The `setup()`/`loop()` model: blinks an LED forever. | LED + resistor (~330 ohm) between GPIO2 and GND |
| [`blink-sos/`](blink-sos/) | Blinks "SOS" in Morse code on the LED. | same as above |
| [`button-led/`](button-led/) | Reads a push button and mirrors it to the LED. | LED on GPIO2, button between GPIO4 and GND |
| [`sd-write/`](sd-write/) | Writes a file to the microSD and reads it back — a quick check that the write path works. | none |
| [`sqlite-notes/`](sqlite-notes/) | PDO opens a SQLite database on the microSD and writes a row each boot. | needs a firmware built with the sqlite extension |
| [`date-timezones/`](date-timezones/) | `DateTime` across named timezones, DST-aware conversions and interval math. | needs the date extension (full timezone db) |
| [`date-utc/`](date-utc/) | `DateTime` in a UTC-only build: what still works and the named zones you give up. | needs the date extension (UTC-only db) |

The linear examples (`hello`, `language-tour`, `require-demo`, `composer-collections`,
`sd-write`, `sqlite-notes`, `date-timezones`, `date-utc`) run once and finish; the hardware
ones use the `setup()`/`loop()` model and keep going as long as the board is powered.

## The examples that need a special firmware

Some examples use optional native extensions that are off by default (native code can't be
side-loaded from the card, so it has to be compiled in). `./flash.sh` asks which optional
extensions to include; each example's README has the exact answers.

- [`sqlite-notes`](sqlite-notes/) — PDO/SQLite (`-DPHP_EXT_SQLITE=ON`).
- [`date-timezones`](date-timezones/) — `ext/date` with the full timezone database
  (`-DPHP_EXT_DATE=ON`).
- [`date-utc`](date-utc/) — `ext/date` with the smaller UTC-only database
  (`-DPHP_EXT_DATE=ON -DPHP_EXT_DATE_MINIMAL_TZ=ON`).

## The two examples that need more than one file

Almost all of them are copied with just their `index.php`. Two are exceptions:

- [`require-demo`](require-demo/) is deliberately made of several files (`index.php`,
  `config.php`, `lib/shapes.php`): copy them to the microSD keeping the same folder layout.
- [`composer-collections`](composer-collections/) needs the whole `vendor/` directory next
  to `index.php`. `vendor/` is not committed: generate it with `composer install` (the
  firmware needs FAT long filenames enabled). Its README walks through every step.
