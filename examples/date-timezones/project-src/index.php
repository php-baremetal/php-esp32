<?php
// The real ext/date with the FULL timezone database: DateTime, named timezones,
// conversions (DST-aware) and interval arithmetic.
//
// Needs a firmware built with the date extension and the full tz database:
// ./flash.sh -> answer "y" to date, and "N" to the UTC-only question (or build
// with idf.py -DPHP_EXT_DATE=ON). Copy this index.php to the microSD.

echo "PHP " . PHP_VERSION . " -- DateTime with timezones\n\n";

// one fixed instant, expressed in UTC
$utc = new DateTimeImmutable('2026-07-29 20:00:00', new DateTimeZone('UTC'));
echo "instant (UTC):  " . $utc->format('Y-m-d H:i:s T') . "\n\n";

// the same instant shown in several named zones -- the tz database knows each
// zone's offset and whether DST is in effect on that date
echo "same instant, around the world:\n";
foreach (['Europe/Rome', 'America/New_York', 'Asia/Tokyo', 'Pacific/Auckland'] as $zone) {
    $local = $utc->setTimezone(new DateTimeZone($zone));
    printf("  %-18s %s\n", $zone, $local->format('Y-m-d H:i:s T (P)'));
}

echo "\n";

// offset difference between two zones at this instant
$rome = new DateTimeZone('Europe/Rome');
$ny   = new DateTimeZone('America/New_York');
$hours = ($rome->getOffset($utc) - $ny->getOffset($utc)) / 3600;
echo "Rome is {$hours}h ahead of New York right now\n";

// interval arithmetic
$later = $utc->add(new DateInterval('P1Y2M10DT2H30M'));
echo "UTC + 1y2m10d2h30m: " . $later->format('Y-m-d H:i:s') . "\n";
