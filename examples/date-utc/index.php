<?php
// The real ext/date, but built with the UTC-only timezone database
// (PHP_EXT_DATE_MINIMAL_TZ). DateTime, formatting, parsing and interval math all
// work -- in UTC. What's gone is the world's named timezones: only UTC is in the
// database, so this shows what you keep and the one thing you give up.
//
// Needs a firmware built with the date extension and the UTC-only tz database:
// ./flash.sh -> answer "y" to date, then "y" to the UTC-only question (or build
// with idf.py -DPHP_EXT_DATE=ON -DPHP_EXT_DATE_MINIMAL_TZ=ON).

echo "PHP " . PHP_VERSION . " -- DateTime in UTC-only mode\n\n";

date_default_timezone_set('UTC');

$now = new DateTimeImmutable('2026-07-29 20:00:00');
echo "instant:     " . $now->format('Y-m-d H:i:s T') . "\n";
echo "spelled out: " . $now->format('l, j F Y') . "\n";   // weekday/month names: still here
echo "unix time:   " . $now->getTimestamp() . "\n\n";

// arithmetic, intervals and diff all work
$deadline = $now->add(new DateInterval('P2W'));            // + 2 weeks
echo "in 2 weeks:  " . $deadline->format('Y-m-d') . "\n";
echo "days apart:  " . $now->diff($deadline)->days . "\n";

// parsing works too
$xmas = new DateTimeImmutable('2026-12-25 09:30:00');
echo "parsed:      " . $xmas->format('D Y-m-d H:i') . "\n\n";

// what you give up: any named timezone. Only UTC exists in this build.
foreach (['UTC', 'Europe/Rome'] as $zone) {
    try {
        new DateTimeZone($zone);
        echo "$zone: available\n";
    } catch (\Exception $e) {
        echo "$zone: not available (UTC-only build)\n";
    }
}
