<?php
// Isolation test: can PHP write a file to the microSD and read it back?
// No extension needed -- file_put_contents / file_get_contents are core.
// Copy this as /index.php, flash any firmware, reset, and read the serial output.

$path = '/sdcard/test.txt';
$data = "hello from PHP " . PHP_VERSION . " tick\n";

echo "writing $path ...\n";
$n = file_put_contents($path, $data);
echo "file_put_contents returned: " . var_export($n, true) . "\n";

clearstatcache();
echo "exists: " . (file_exists($path) ? "yes" : "no") . "\n";
echo "size:   " . (file_exists($path) ? filesize($path) : "-") . "\n";

echo "--- read back (in the same run) ---\n";
$back = file_get_contents($path);
echo var_export($back, true) . "\n";
echo "matches what we wrote: " . ($back === $data ? "YES" : "NO") . "\n";

// append test, to exercise a second open/write
file_put_contents($path, "second line\n", FILE_APPEND);
echo "--- after append ---\n";
echo file_get_contents($path);
