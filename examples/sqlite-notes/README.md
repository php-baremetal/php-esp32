# sqlite-notes

A tiny database on the microcontroller: PDO opens a SQLite file on the microSD, writes a
row on every boot, and reads the rows back. Because the `.db` lives on the card, the data
survives resets — the boot counter keeps going up.

## This one needs a special firmware

PDO/SQLite is **not** in the default build (it's an optional native extension, and native
code can't be side-loaded from the card — it has to be compiled into the firmware). This
project's `php-esp32.config.toml` enables it (`[extensions.sqlite]`), so `phpflash build`
compiles it in with no flags to pass. The first build fetches the SQLite amalgamation
(`scripts/fetch-sqlite.sh`, kept out of git) for you.

## What it does

```php
$pdo = new PDO('sqlite:file:/sdcard/notes.db?nolock=1');
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
$pdo->exec('PRAGMA journal_mode = MEMORY');
$pdo->exec('PRAGMA synchronous = OFF');

$pdo->exec('CREATE TABLE IF NOT EXISTS boots (id INTEGER PRIMARY KEY AUTOINCREMENT, note TEXT)');
$pdo->prepare('INSERT INTO boots (note) VALUES (?)')->execute(['booted PHP ' . PHP_VERSION]);
```

Two choices are deliberate, and specific to running on this hardware:

- **`?nolock=1`** in the DSN — FATFS has no POSIX file locking, and the board is a single
  process, so locking is neither available nor needed.
- **`journal_mode = MEMORY` + `synchronous = OFF`** — the rollback journal is kept in RAM
  instead of a `-journal` file on the card, and commits don't `fsync`. This keeps SQLite off
  the parts of the filesystem story that a card with no real sync semantics doesn't provide.

## Expected output

The database file is created on the first run (PDO opens SQLite with "create if
missing"), then reused. Each reset adds a row, so the count climbs:

```
# first boot
created a new database at /sdcard/notes.db
boots recorded so far: 1
last 5:
  #1  booted PHP 8.3.32

# later boots
opened existing database /sdcard/notes.db
boots recorded so far: 4
last 5:
  #4  booted PHP 8.3.32
  #3  booted PHP 8.3.32
  #2  booted PHP 8.3.32
  #1  booted PHP 8.3.32
```

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

To run from a microSD, copy `project-src/index.php` to the card root and press reset. The
`notes.db` file is created on the card on the first run and reused afterwards. No wiring needed.
