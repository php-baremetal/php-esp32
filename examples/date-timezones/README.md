# date-timezones

The real `ext/date` with the full timezone database: `DateTime`, named timezones, DST-aware
conversions, and interval arithmetic — the same date handling you'd have on a server.

## This one needs the date extension (full timezone db)

`ext/date` is off by default. This project's `php-esp32.config.toml` enables it
(`[extensions.date]`, full timezone database — `minimal_tz` left off), so `phpflash build`
compiles it in with no flags to pass. The full timezone database adds ~650 KB of flash. (If you
only need UTC, see the [`date-utc`](../date-utc/) example and its smaller build.)

## What it does

Takes one fixed instant in UTC and shows it in several named zones — the database knows each
zone's offset and whether daylight saving is in effect on that date — then does some offset
math and interval arithmetic.

```php
$utc = new DateTimeImmutable('2026-07-29 20:00:00', new DateTimeZone('UTC'));
foreach (['Europe/Rome', 'America/New_York', 'Asia/Tokyo', 'Pacific/Auckland'] as $zone) {
    $local = $utc->setTimezone(new DateTimeZone($zone));
    printf("  %-18s %s\n", $zone, $local->format('Y-m-d H:i:s T (P)'));
}
```

## Expected output

```
instant (UTC):  2026-07-29 20:00:00 UTC

same instant, around the world:
  Europe/Rome        2026-07-29 22:00:00 CEST (+02:00)
  America/New_York   2026-07-29 16:00:00 EDT (-04:00)
  Asia/Tokyo         2026-07-30 05:00:00 JST (+09:00)
  Pacific/Auckland   2026-07-30 08:00:00 NZST (+12:00)

Rome is 6h ahead of New York right now
UTC + 1y2m10d2h30m: 2027-10-09 22:30:00
```

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

To run from a microSD instead, copy `project-src/index.php` to the card root and press
reset. No wiring needed.
