# sqlite3-notes

The same tiny on-device database as [`sqlite-notes`](../sqlite-notes/), but through the **SQLite3
class** (`ext/sqlite3`) instead of **PDO**. It opens a SQLite file on the microSD, writes a row on
every boot, and reads the rows back; the `.db` lives on the card, so the boot counter survives resets.

## Choosing the API

The `sqlite` extension builds **PDO + `pdo_sqlite`** by default. This project sets

```toml
[extensions.sqlite]
enabled = true
type    = "sqlite3"   # the SQLite3-class API (ext/sqlite3); default is "pdo-sqlite"
```

so `phpflash build` compiles `ext/sqlite3` instead. Same SQL engine (the amalgamation is shared), a
different API surface — and about **45 KB smaller**, because it skips the PDO layer. Use whichever API
you prefer; pick `pdo-sqlite` if you want portable PDO code, `sqlite3` if you want the leaner build.

Either way it's an optional native extension (native code can't be side-loaded from the card), so it
has to be compiled in — the first build fetches the SQLite amalgamation (`scripts/fetch-sqlite.sh`,
kept out of git) for you.

## What it does

```php
$db = new SQLite3('file:/sdcard/notes.db?nolock=1',
                  SQLITE3_OPEN_READWRITE | SQLITE3_OPEN_CREATE);
$db->enableExceptions(true);
$db->exec('PRAGMA journal_mode = MEMORY');
$db->exec('PRAGMA synchronous = OFF');

$db->exec('CREATE TABLE IF NOT EXISTS boots (id INTEGER PRIMARY KEY AUTOINCREMENT, note TEXT)');
$stmt = $db->prepare('INSERT INTO boots (note) VALUES (?)');
$stmt->bindValue(1, 'booted PHP ' . PHP_VERSION);
$stmt->execute();
```

Two choices are deliberate, and specific to this hardware:

- **`?nolock=1`** in the `file:` URI — FATFS has no POSIX file locking, and the board is a single
  process, so locking is neither available nor needed. (`SQLITE_USE_URI` is compiled in, so a
  `file:` name is treated as a URI.)
- **`journal_mode = MEMORY` + `synchronous = OFF`** — the rollback journal is kept in RAM instead of
  a `-journal` file on the card, and commits don't `fsync`.

## Expected output

```
# first boot
created a new database at /sdcard/notes.db
boots recorded so far: 1
last 5:
  #1  booted PHP 8.4.25

# later boots
opened existing database /sdcard/notes.db
boots recorded so far: 3
last 5:
  #3  booted PHP 8.4.25
  #2  booted PHP 8.4.25
  #1  booted PHP 8.4.25
```

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

To run from a microSD, copy `project-src/index.php` to the card root and press reset. The `notes.db`
file is created on the card on the first run and reused afterwards. No wiring needed.
