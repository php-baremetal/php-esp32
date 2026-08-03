# require-demo

A small program split across several files with `require`, the plain PHP way to reuse
code without any tooling. It shows that the engine on the chip resolves and loads other
files from the SD card, not just a single `index.php`.

## What it does

`index.php` is the entry point. It pulls in a file of class definitions with
`require_once`, and a config file that *returns* a value with `require`, then uses both.

```
require-demo/
  index.php        entry point
  config.php       returns an array
  lib/shapes.php   Shape / Circle / Rectangle classes
```

```php
require_once __DIR__ . '/lib/shapes.php';   // class definitions
$config = require __DIR__ . '/config.php';  // a file that returns a value

echo "app: {$config['name']} v{$config['version']}\n";
```

`__DIR__` is the directory the running script lives in, which is `/sdcard` on the board,
so the `require` paths resolve to the files sitting next to `index.php` on the card.

## What it demonstrates

- **The interpreter loads more than one file from the SD.** `require`/`require_once` open,
  compile and run other files off the card, exactly as on a normal PHP host.
- **Both `require` idioms work.** `require_once` for declarations (so classes are defined
  only once), and `require` on a file that `return`s a value, the common config pattern.
- **Relative paths behave.** `__DIR__` points at the script's own folder, including a
  subdirectory (`lib/`), so a normal project layout carries over unchanged.

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

To run from a microSD instead, copy the contents of `project-src/` to the card root,
keeping the layout (`/index.php`, `/config.php`, `/lib/shapes.php`). The entry point
stays `index.php`. No wiring needed. (These filenames are short on purpose, so this
also works on a firmware built without FAT long-filename support.)
