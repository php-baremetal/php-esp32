# composer-collections

Real third-party code, pulled in through Composer's autoloader, running on the chip. This
example uses the [Illuminate Collections](https://packagist.org/packages/illuminate/collections)
package (the `Collection` class from Laravel) to crunch a small set of sensor readings.

## What it does

`index.php` requires Composer's generated `vendor/autoload.php` and then just uses the
package. No manual `require` per class: Composer's autoloader loads each class from
`vendor/` the first time it is referenced.

```php
require __DIR__ . '/vendor/autoload.php';

use Illuminate\Support\Collection;

$averages = $readings
    ->groupBy('sensor')
    ->map(fn (Collection $group) => round($group->avg('value'), 2));
```

The methods used (`groupBy`, `map`, `avg`, `sortByDesc`, `take`, `pluck`, `sum`) are all
array-based, so the example stays clear of the string helpers that would pull in the
`mbstring` extension (which this build doesn't include).

## What it demonstrates

- **Composer autoloading works on a microcontroller.** `spl_autoload_register`, the
  generated class map and the on-demand `require` of files from `vendor/` all run on the
  RISC-V core. Sixteen vendor files end up loaded to satisfy this script.
- **An unmodified third-party library runs as-is.** Illuminate Collections is stock code
  from Packagist; nothing about it was changed for the chip.

## The output

```
PHP 8.3.32 on ESP32-P4
--- /sdcard/index.php ---
averages by sensor:
  temp      22.17
  humidity  49.57
top 3 values:  51.5, 49.2, 48
reading count: 6
overall sum:   215.2
--- end ---
php-esp32: done -- heap free: 33978143 bytes
```

## A port fix this example needed

Getting here turned up a real bug in the port. Frameworks lean hard on scope-bound
closures, and on this build (no opcache, `USE_ZEND_ALLOC=0`) the engine's per-closure
`efree()` of a closure's run-time cache corrupted the heap. The fix, carried as a patch in
`components/php/patches/`, lets the request arena own that cache instead of freeing it one
closure at a time. The trade-off: the cache isn't reclaimed per closure, so a script that
creates many closures inside a forever `loop()` would grow memory until the run ends. For
one-shot scripts like this one it makes no difference.

## Building vendor/

`vendor/` is not committed (that's third-party code); reproduce it with Composer. You need
Composer and a PHP 8.2+ on your computer:

```
cd examples/composer-collections/project-src
composer install --no-dev
```

`composer.json` and `composer.lock` are committed, so `install` pins the exact same
versions that were tested here.

## Building and running

After generating `vendor/` (above):

```sh
phpflash build && phpflash flash && phpflash monitor
```

The `vendor/` tree has names longer than the old 8.3 limit (e.g.
`ShouldHandleEventsAfterCommit.php`), so the firmware needs FAT long filenames — already
enabled in the default `sdkconfig`. To run from a microSD instead, copy the contents of
`project-src/` (both `index.php` and the whole `vendor/`) to the card root and press reset.
No wiring needed.
