# eloquent-demo

Laravel's **Eloquent** ORM running on the microcontroller — standalone, *without* the Laravel
framework. It uses the `illuminate/database` package's Capsule manager, defines a `Post` model,
creates its table with the schema builder, and writes one row on every boot to a SQLite
database on the microSD. The row survives resets because the `.db` file is on the card.

It exercises the whole Eloquent stack: PDO/SQLite for storage, the `Str::` helpers (mbstring)
for model/table naming, and Carbon (`ext/date`) for the `created_at`/`updated_at` timestamps.

This is the **without-oniguruma** half of a pair: mbstring here has no regex engine, so the
example polyfills the one function Eloquent needs (`mb_split`) over PCRE — see the note below.
Its twin, [`eloquent-onig`](../eloquent-onig/), is the same demo on a firmware built *with*
oniguruma, where `mb_split` is native and no polyfill is needed.

## Firmware

This is the "everything" example — build a firmware with **all** of these on:

- `./flash.sh` → answer **y** to `sqlite`, `mbstring` (then **y** to *drop CJK*, Eloquent
  doesn't need it), `ctype`, `filter` and `date`, or
- `idf.py -DPHP_EXT_SQLITE=ON -DPHP_EXT_MBSTRING=ON -DPHP_EXT_MBSTRING_NO_CJK=ON -DPHP_EXT_CTYPE=ON -DPHP_EXT_FILTER=ON -DPHP_EXT_DATE=ON ...`

Dropping the CJK codecs (`MBSTRING_NO_CJK`) keeps the image small; the full timezone database
isn't required either, so `-DPHP_EXT_DATE_MINIMAL_TZ=ON` is fine too (the demo pins UTC).

## Producing `vendor/`

`vendor/` isn't committed — generate it on your PC with Composer, then copy the whole folder to
the card next to `index.php` (the firmware needs FAT long filenames, which it has by default):

```
cd examples/eloquent-demo
composer install
```

`composer.json` pins the platform to **PHP 8.3.32** (the version on the board), so Composer
resolves dependencies that actually run there — e.g. `symfony/translation` v7, not the v8 that
requires PHP 8.4. `composer.lock` is committed so you get exactly that resolution.

## A note on `mb_split`

`mbstring` on this board is built without the oniguruma regex engine (it's a separate external
library PHP doesn't bundle), so the `mb_ereg*` / `mb_split` family isn't compiled in — see
[`docs/reference/footprint.md`](../../docs/reference/footprint.md) and the porting notes. Eloquent's `Str` helpers
call `mb_split('\s+', …)` in a few places, so `index.php` defines a tiny `mb_split()` polyfill
backed by PCRE (which is always in). It's guarded by `function_exists`, so on a PC with native
`mb_split` it does nothing. For the simple whitespace patterns Eloquent uses, PCRE is exact.

## A note on SQLite and FATFS

Eloquent's built-in SQLite connector opens the file with plain POSIX locking and runs
`realpath()` on the path first. FATFS has neither, so the example hands Eloquent its own PDO —
a `file:` URI with `?nolock=1` — via `Capsule::getConnection()->setPdo(...)`. Everything above
that (models, query builder, schema) is untouched, ordinary Eloquent.

## Run

Copy this folder (with `vendor/`) to the microSD, reset the board, watch the serial port.

## Output (excerpt, first two boots)

```
created a new database at /sdcard/eloquent.db
inserted post #1 at 2026-07-31 07:50:38
total posts: 1
last 5:
  #1  boot #1   2026-07-31 07:50:38
```
```
opened existing database /sdcard/eloquent.db
inserted post #2 at 2026-07-31 07:50:39
total posts: 2
last 5:
  #2  boot #2   2026-07-31 07:50:39
  #1  boot #1   2026-07-31 07:50:38
```
