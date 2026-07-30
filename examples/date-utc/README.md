# date-utc

The real `ext/date`, but built with the **UTC-only** timezone database
(`PHP_EXT_DATE_MINIMAL_TZ`). `DateTime`, formatting, parsing and interval math all still work
— in UTC. What you give up is the world's named timezones: only UTC is in the database. This
example shows what you keep and the one thing you lose, versus
[`date-timezones`](../date-timezones/).

## This one needs the date extension (UTC-only db)

```
./flash.sh          # answer "y" to date, then "y" to the UTC-only question
```

or directly:

```
idf.py -DPHP_EXT_DATE=ON -DPHP_EXT_DATE_MINIMAL_TZ=ON flash
```

The UTC-only database is ~350 KB smaller than the full one, so the whole date extension costs
about 300 KB of flash instead of 650 KB.

## What it does

```php
date_default_timezone_set('UTC');

$now = new DateTimeImmutable('2026-07-29 20:00:00');
echo $now->format('l, j F Y') . "\n";               // weekday/month names: still here
echo $now->add(new DateInterval('P2W'))->format('Y-m-d') . "\n";   // arithmetic works

new DateTimeZone('Europe/Rome');                    // this throws: not in a UTC-only build
```

Weekday and month names, parsing, formatting, `DateInterval`, `diff()` — none of those need
the timezone database, so they all work. Only looking up a *named* zone fails.

## Expected output

On a UTC-only firmware:

```
instant:     2026-07-29 20:00:00 UTC
spelled out: Wednesday, 29 July 2026
unix time:   1785355200

in 2 weeks:  2026-08-12
days apart:  14
parsed:      Fri 2026-12-25 09:30

UTC: available
Europe/Rome: not available (UTC-only build)
```

The last two lines are the whole point: `UTC` is there, any named zone isn't. On a full-tz
firmware (the `date-timezones` example) that last line would read `available` instead — the
difference is the build, not the code.

## Running it

Build and flash a firmware with date + the UTC-only db (above). Copy `index.php` to the
microSD as `/index.php`, put the card back, press reset. No wiring needed.
