# eloquent-onig

The same standalone **Eloquent** demo as [`eloquent-demo`](../eloquent-demo/), but for a firmware
where `mbstring` is built **with oniguruma** — so `mb_split()` (which Eloquent's `Str` helpers
call) is the native function and **no polyfill is needed**. This is the "with oniguruma" half of
the pair; `eloquent-demo` is the "without oniguruma" half that polyfills `mb_split` over PCRE.

Everything else is identical: the Capsule manager, a `Post` model, the schema builder, a row per
boot on a SQLite database on the microSD (here `eloquent-onig.db`, so it doesn't clash with the
other example's file).

## Firmware

Build with the whole stack **and** the mbstring regex engine:

- `./flash.sh` → answer **y** to `sqlite`, `mbstring` (then **y** to *mb_ereg\*/mb_split regex*),
  `ctype`, `filter`, `date`, or
- `idf.py -DPHP_EXT_SQLITE=ON -DPHP_EXT_MBSTRING=ON -DPHP_EXT_MBSTRING_ONIG=ON -DPHP_EXT_CTYPE=ON -DPHP_EXT_FILTER=ON -DPHP_EXT_DATE=ON ...`
  (run `./scripts/fetch-oniguruma.sh` first).

The oniguruma engine adds ~445 KB over plain mbstring (see [`docs/reference/footprint.md`](../../docs/reference/footprint.md)).
If you'd rather not pay that, use [`eloquent-demo`](../eloquent-demo/) instead — same ORM, with a
tiny `mb_split` polyfill.

## Producing `vendor/`

Same as the other example — `vendor/` isn't committed:

```
cd examples/eloquent-onig/project-src
composer install
```

`composer.json` pins the platform to PHP 8.3.32 (the board's version) so the dependency
resolution matches what runs there; `composer.lock` is committed.

## Run

Copy `project-src/` (with `vendor/`) to the microSD, reset the board, watch the serial port.

## Output (excerpt, first two boots)

```
created a new database at /sdcard/eloquent-onig.db
mb_split available: yes (native)
inserted post #1 at 2026-07-31 08:30:32
total posts: 1
last 5:
  #1  boot #1   2026-07-31 08:30:32
```
```
opened existing database /sdcard/eloquent-onig.db
mb_split available: yes (native)
inserted post #2 at 2026-07-31 08:30:32
total posts: 2
last 5:
  #2  boot #2   2026-07-31 08:30:32
  #1  boot #1   2026-07-31 08:30:32
```
