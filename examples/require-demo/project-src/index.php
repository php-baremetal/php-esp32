<?php
// Splitting a program across several files with require. This is the plain,
// no-Composer way to reuse code: name a file and pull it in.
//
// Copy this whole folder's contents to the microSD, keeping the layout:
//   /index.php
//   /config.php
//   /lib/shapes.php
// require paths are relative to __DIR__, which is /sdcard when it runs.

require_once __DIR__ . '/lib/shapes.php';   // brings in the class definitions
$config = require __DIR__ . '/config.php';  // a file that returns a value

echo "app: {$config['name']} v{$config['version']}\n";

$shapes = [
    new Circle(2.0),
    new Rectangle(3.0, 4.0),
    new Circle(1.5),
];

$total = 0.0;
foreach ($shapes as $shape) {
    printf("%-10s area = %.4f\n", $shape->name(), $shape->area());
    $total += $shape->area();
}
printf("%-10s area = %.4f\n", 'total', $total);
