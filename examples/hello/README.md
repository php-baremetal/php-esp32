# hello

The smallest possible example: a plain script that runs top to bottom and stops. If this
prints, the engine is alive on the chip.

## What it does

`index.php` echoes a line, does some arithmetic, asks the runtime how much memory it is
using, and loops over an array. No functions, no classes, nothing clever, on purpose.

```php
echo "Hello from PHP " . PHP_VERSION . " on an ESP32-P4!\n";
echo "2 ** 16 = " . (2 ** 16) . "\n";
echo "memory in use: " . memory_get_usage() . " bytes\n";

foreach (['world', 'chip', 'microcontroller'] as $who) {
    echo "hello, $who\n";
}
```

## What it demonstrates

- **The interpreter runs a real script from the SD card.** The board reads `/index.php`,
  compiles it, and runs it: `PHP 8.3.32 on ESP32-P4` is the genuine engine reporting in.
- **A linear script starts and finishes cleanly.** No `setup()`/`loop()` here; the script
  ends and the board reports `done`. This is the plain execution path.
 
## The output

From [`monitor.txt`](monitor.txt) (the full serial log):

```
PHP 8.3.32 on ESP32-P4
--- /sdcard/index.php ---
Hello from PHP 8.3.32 on an ESP32-P4!
2 ** 16 = 65536
memory in use: 0 bytes
hello, world
hello, chip
hello, microcontroller
--- end ---
php-esp32: done -- heap free: 33980391 bytes
```

`memory in use: 0 bytes` is expected, not a bug: this build runs with `USE_ZEND_ALLOC=0`,
so allocations go straight to the system allocator instead of Zend's memory manager, and
`memory_get_usage()` (which only counts what that manager hands out) sees nothing.

## Running it

Copy `index.php` to the microSD (in the root, as `index.php`), put the card back in the
board, and press reset. No wiring needed.
