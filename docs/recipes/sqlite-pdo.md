---
eyebrow: 'Docs · Recipes'
lede:    'Open a SQLite database from PHP with PDO and run CREATE, INSERT and SELECT — on a file on the microSD that survives resets, or an in-memory database that does not. PDO and the SQLite driver are built into the firmware.'
see_also:
  - { href: '../extensions/porting-status.md', meta: '6 min' }
  - { href: './microsd-files.md', meta: '5 min' }
  - { href: './web-page.md', meta: '6 min' }
prev: { label: 'Serve a web page', href: './web-page.md' }
next: { label: 'Make an HTTPS request', href: './https-request.md' }
---

# Store data in SQLite with PDO

SQLite is the natural database for a microcontroller: no server, no network, the whole engine and the data in one file. On php-esp32 you reach it through PDO — `ext/pdo` and `ext/pdo_sqlite` are built together from one flag, and the SQLite amalgamation is compiled straight into the firmware. This recipe opens a database, creates a table, writes a row on every boot, and reads it back.

## What you need

- A board flashed with php-esp32 (the examples target the ESP32-P4-Pico).
- The `sqlite` extension enabled at build time — one flag turns on both `ext/pdo` and `ext/pdo_sqlite`. Nothing else is required; SQLite has no external dependencies.
- A microSD card *if* you want the database to persist across resets. An in-memory database needs no card.

<!-- @callout variant="note" title="PDO is the only SQLite API" -->
The standalone `SQLite3` class is not built. Use the PDO driver — `new PDO('sqlite:...')` — for everything. That is the same API the Eloquent example below runs on.
<!-- @endcallout -->

## Two locations for the database

A SQLite database is either a file on disk or a scratch database that lives only in RAM. Pick by the DSN you pass to `PDO`.

<!-- @tabs labels="On the microSD, In memory" -->

<!-- @tab index="0" -->

`sqlite:/sdcard/app.db` opens (or creates) a file on the card mounted at `/sdcard`. The file — and every row in it — survives resets and power cycles. This is what you want for anything you plan to keep.

<!-- @code-block language="php" label="a file on the card" -->
```php
$pdo = new PDO('sqlite:/sdcard/app.db');
```
<!-- @endcode-block -->

<!-- @endtab -->

<!-- @tab index="1" -->

`sqlite::memory:` creates a fresh database in RAM. It is fast and needs no microSD, but it is gone the moment the program ends or the board resets. Good for scratch computation, tests, or when the card is read-only.

<!-- @code-block language="php" label="in memory" -->
```php
$pdo = new PDO('sqlite::memory:');
```
<!-- @endcode-block -->

<!-- @endtab -->

<!-- @endtabs -->

<!-- @callout variant="warning" title="Where the file can live" -->
A writable database file must sit on the microSD (`/sdcard/...`). If your project uses an **embedded** source image instead of a card, that image is read-only at runtime — you can ship a pre-populated `.db` and read from it, but you cannot `INSERT` into it. To write, either use the microSD or an `::memory:` database.
<!-- @endcallout -->

## The code

Put this in `project-src/index.php`. It opens a database on the card, creates a `notes` table on the first run, records one row per boot, and prints the last few back with a prepared `SELECT`.

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php

$dbfile = '/sdcard/app.db';
$fresh  = !file_exists($dbfile);

$pdo = new PDO('sqlite:' . $dbfile);
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

echo $fresh
    ? "created a new database at $dbfile\n"
    : "opened existing database $dbfile\n";

// Create the table once, on the first boot.
$pdo->exec('CREATE TABLE IF NOT EXISTS notes (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    body    TEXT NOT NULL,
    created TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
)');

// Count what is there, then record this boot as a new row.
$n = (int) $pdo->query('SELECT COUNT(*) FROM notes')->fetchColumn();

$insert = $pdo->prepare('INSERT INTO notes (body) VALUES (:body)');
$insert->execute([':body' => 'boot #' . ($n + 1)]);
echo "inserted note #" . $pdo->lastInsertId() . "\n";

// Read the last five back, newest first.
echo "total notes: " . ($n + 1) . "\n";
echo "last 5:\n";
$rows = $pdo->query('SELECT id, body, created FROM notes ORDER BY id DESC LIMIT 5');
foreach ($rows as $row) {
    printf("  #%d  %-8s  %s\n", $row['id'], $row['body'], $row['created']);
}
```
<!-- @endcode-block -->

Everything here is stock PDO: a DSN to open the connection, `exec()` for statements with no result, `prepare()`/`execute()` with bound parameters for the write, and `query()` returning an iterable of rows. Turning on `PDO::ERRMODE_EXCEPTION` means a SQL error throws instead of failing silently.

<!-- @callout variant="note" title="FATFS and file locking" -->
The microSD is a FAT filesystem, which has no POSIX file locking. For plain PDO writes this is fine. If a library opens the database its own way and trips over locking or `realpath()`, open the file yourself with a `file:` URI and `?nolock=1` — for example `new PDO('sqlite:file:/sdcard/app.db?nolock=1')` — and hand that PDO to the library. The `eloquent-demo` example does exactly this.
<!-- @endcallout -->

## Config

A minimal microSD project with the SQLite extension enabled. The one `[extensions.sqlite]` table builds both `ext/pdo` and `ext/pdo_sqlite`; `phpflash` fetches the SQLite amalgamation on demand.

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
name = "sqlite-notes"
storage_type = "microsd"   # writable database file lives on the card
type = "cli"

[board]
target = "esp32-p4-pico"
port   = ""                # empty = autodetect at flash time

[php]
src   = "project-src"      # PHP source folder
entry = "index.php"        # entry file within src

[extensions.sqlite]
enabled = true             # builds ext/pdo + ext/pdo_sqlite
```
<!-- @endcode-block -->

## Build & flash

Build the firmware, write it to the board, then open the serial monitor.

<!-- @code-block language="bash" label="build, flash, monitor" -->
```bash
phpflash build && phpflash flash && phpflash monitor
```
<!-- @endcode-block -->

With `storage_type = "microsd"` the PHP source and the database both live on the card. To try a change to `index.php`, copy it to the card root and press reset — no rebuild needed. The `.db` file stays where it is, so the rows you already wrote are still there.

## What you'll see

The first boot creates the database and writes note #1; every reset after that opens the existing file and appends another row. Because the file is on the card, the count keeps climbing across power cycles.

<!-- @code-block language="text" label="serial output (first two boots)" -->
```text
PHP 8.3.32 on ESP32-P4
--- /sdcard/index.php ---
created a new database at /sdcard/app.db
inserted note #1
total notes: 1
last 5:
  #1  boot #1   2026-07-31 07:50:38
--- end ---

[ REBOOT ]

opened existing database /sdcard/app.db
inserted note #2
total notes: 2
last 5:
  #2  boot #2   2026-07-31 07:50:39
  #1  boot #1   2026-07-31 07:50:38
```
<!-- @endcode-block -->

## Going further: Eloquent

Because it is real PDO underneath, an ORM works too. The `eloquent-demo` example runs Laravel's Eloquent (`illuminate/database`) *standalone*, without the Laravel framework: the Capsule manager, a `Post` model, the schema builder, and the same SQLite file on the card.

<!-- @code-block language="php" label="eloquent-demo/index.php (excerpt)" -->
```php
require __DIR__ . '/vendor/autoload.php';

use Illuminate\Database\Capsule\Manager as Capsule;
use Illuminate\Database\Eloquent\Model;

$capsule = new Capsule;
$capsule->addConnection(['driver' => 'sqlite', 'database' => '/sdcard/eloquent.db']);
$capsule->setAsGlobal();
$capsule->bootEloquent();

// Hand Eloquent a PDO opened with ?nolock=1 so it plays with FATFS.
$pdo = new PDO('sqlite:file:/sdcard/eloquent.db?nolock=1');
$pdo->exec('PRAGMA journal_mode = MEMORY');
$capsule->getConnection()->setPdo($pdo);

class Post extends Model { protected $fillable = ['title']; }

$post = Post::create(['title' => 'boot #' . (Post::count() + 1)]);
echo "inserted post #{$post->id} at {$post->created_at}\n";
```
<!-- @endcode-block -->

<!-- @callout variant="tip" title="Composer and the everything firmware" -->
Eloquent needs its dependencies and a fuller extension set. Run `composer install` on your PC and copy the whole `vendor/` folder to the card next to `index.php` (the firmware has FAT long filenames on by default). The build must also enable `mbstring` (the `Str::` helpers), `ctype`, `filter` and `date` (Carbon timestamps) alongside `sqlite`. See `examples/eloquent-demo/README.md` for the exact flags and the one-line `mb_split` polyfill it uses when `mbstring` is built without oniguruma.
<!-- @endcallout -->
