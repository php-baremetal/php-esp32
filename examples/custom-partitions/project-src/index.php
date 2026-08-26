<?php
// A tiny sketch for the ESP32-S3-Zero (4 MB flash / 2 MB PSRAM) that ships its OWN partition
// table (see ../partitions.csv) and keeps a reboot-persistent boot counter.
//
// The point of the example: on a tight-flash board the board's default `factory` may leave too
// little room, or you may want a different split. `phpflash partitions publish` drops the board's
// table into the project as partitions.csv; edit it (here `factory` is 3200K instead of 3456K) and
// the build uses it. The `storage` (embedded source) and `phpstore` (this store) partitions are
// still sized and appended automatically.

echo "PHP " . PHP_VERSION . " on the ESP32-S3-Zero\n";
echo "free heap: " . number_format(memory_get_usage()) . " bytes\n";

// store_* is backed by the `phpstore` NVS partition ([store] size_kb in the config). It survives
// resets and power cycles -- so this count goes up every time the board boots.
$boots = (int) store_get('boots', '0') + 1;
store_set('boots', (string) $boots);
echo "boot #$boots since the store was created\n";
