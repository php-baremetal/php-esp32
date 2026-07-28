<?php
// Composer autoloading on a microcontroller. This requires Composer's generated
// autoloader and then uses the Illuminate Collections package: real third-party
// code, with its classes loaded on demand by the autoloader as they are touched.
//
// Copy this folder to the microSD keeping the layout (index.php + the whole
// vendor/ directory). See the README for how vendor/ is produced and why the
// firmware needs FAT long filenames turned on.

require __DIR__ . '/vendor/autoload.php';

use Illuminate\Support\Collection;

$readings = new Collection([
    ['sensor' => 'temp',     'value' => 21.4],
    ['sensor' => 'temp',     'value' => 22.1],
    ['sensor' => 'humidity', 'value' => 48.0],
    ['sensor' => 'humidity', 'value' => 51.5],
    ['sensor' => 'temp',     'value' => 23.0],
    ['sensor' => 'humidity', 'value' => 49.2],
]);

// group by sensor, average each group -- all array-based, no string helpers
$averages = $readings
    ->groupBy('sensor')
    ->map(fn (Collection $group) => round($group->avg('value'), 2));

echo "averages by sensor:\n";
foreach ($averages as $sensor => $avg) {
    printf("  %-9s %s\n", $sensor, $avg);
}

$hottest = $readings->sortByDesc('value')->take(3)->pluck('value')->all();
echo "top 3 values:  " . implode(', ', $hottest) . "\n";
echo "reading count: " . $readings->count() . "\n";
echo "overall sum:   " . $readings->sum('value') . "\n";
