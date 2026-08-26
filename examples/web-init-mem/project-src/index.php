<?php
/*
 * Per request. Each HTTP request is a fresh PHP cycle, so userland does not persist -- but the
 * in-RAM mem_* store does. We read the value init.php seeded and keep a request counter in RAM
 * (no flash wear, unlike store_*).
 */
$hits = (int) mem_get('hits', 0) + 1;
mem_set('hits', $hits);

header('Content-Type: text/plain');
echo mem_get('boot_msg', '(unset)'), "\n";
echo "request #$hits since boot\n";
echo "mem keys: ", implode(', ', mem_keys()), "\n";
