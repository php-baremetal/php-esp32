---
eyebrow: 'Docs · Recipes'
lede: 'Write a file to the microSD card from PHP and read it back with the plain file functions you already know — file_put_contents, fopen, fgets — all pointed at the /sdcard mount.'
see_also:
  - href: ../getting-started/quick-start.md
    meta: 'Getting started'
    label: 'Quick start'
  - href: ../storage/persistent-store.md
    meta: 'Storage & state'
    label: 'Persistent store (store_*)'
  - href: ./web-page.md
    meta: 'Recipes'
    label: 'Serve a web page'
prev:
  label: 'Drive an SSD1306 OLED'
  href: ./ssd1306-oled.md
next:
  label: 'Serve a web page'
  href: ./web-page.md
---

# Read and write files on the microSD

The microSD card is mounted read-write at `/sdcard`, so every core PHP file function works against it unchanged. There is no special API and no extension to enable: `file_put_contents`, `file_get_contents`, `fopen`/`fgets`, `filesize`, `scandir` — all of them take a `/sdcard/...` path and behave exactly as they do on a desktop.

This recipe is built on the [`sd-write`](https://github.com/php-baremetal/php-esp32/tree/master/examples/sd-write) example: it writes a string to `/sdcard/test.txt`, reads it back in the same run to confirm the write path, then appends a second line.

## Goal

Write a file to the card, read it back, append to it, and list a directory — using nothing but the standard library.

## What you need

- A board with a microSD slot (any P4 or S3 `-Pico` / `-ETH`; the `-Zero` variants have no slot).
- A **FAT-formatted microSD card**, seated in the slot before you power the board.

No wiring and no extension: the file functions are part of core PHP.

## Where the card mounts

The firmware mounts the card at `/sdcard` during boot, and the serial log confirms it:

<!-- @code-block language="text" label="Boot log — the card came up" -->
```
php-esp32: microSD mounted at /sdcard
```
<!-- @endcode-block -->

Every path in your script is absolute from that mount point — the card's root is `/sdcard/`, so a file called `test.txt` at the card's top level is `/sdcard/test.txt`.

### microSD vs embedded storage

Where the card is mounted is independent of where your *code* runs from. That is the project's `storage_type`:

- **`microsd`** — your `project-src/` is copied to the card and the entry script is read from `/sdcard/index.php` at boot. This is the fastest edit loop (pull the card, edit, reinsert) and the case this recipe uses. The card is already mounted, so reading and writing other files on it is free.
- **`embedded`** — your source is baked into the firmware image, which is read-only. If embedded code needs to write files, mount a card alongside it by adding `[storage] microsd = true`; the code is served from flash while writes go to `/sdcard`.

<!-- @code-block language="toml" label="An embedded project that also mounts a card for writes" -->
```toml
storage_type = "embedded"

[storage]
microsd = true
```
<!-- @endcode-block -->

Either way, once the card is mounted the file code below is identical.

## The code

The full entry script from the example. It writes, reads back and compares in the same run, then appends a second line:

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
// Can PHP write a file to the microSD and read it back?
// No extension needed -- file_put_contents / file_get_contents are core.

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
```
<!-- @endcode-block -->

A few things worth noting:

- `file_put_contents` opens, writes and closes in one call; passing `FILE_APPEND` reopens the file and adds to the end instead of truncating.
- Call `clearstatcache()` before `file_exists`/`filesize` if you just wrote the file — PHP caches stat results within a request.
- The read-back comparison (`$back === $data`) is the actual proof the write reached the card and came back byte-for-byte.

### Streaming line by line

For a larger file, read it a line at a time instead of slurping it whole, and list a directory with `scandir`:

<!-- @code-block language="php" label="Line-by-line read and a directory listing" -->
```php
$fh = fopen('/sdcard/test.txt', 'r');
while (($line = fgets($fh)) !== false) {
    echo "> " . $line;
}
fclose($fh);

foreach (scandir('/sdcard') as $entry) {
    if ($entry === '.' || $entry === '..') {
        continue;
    }
    echo $entry . "\n";
}
```
<!-- @endcode-block -->

## Config

The example runs its source straight from the card, so `storage_type` is `microsd`:

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
name = "sd-write"
storage_type = "microsd"   # where the PHP source lives
type = "init-loop"         # execution model

[board]
target = "esp32-p4-pico"
port   = ""                # empty = autodetect at flash time

[php]
src   = "project-src"      # PHP source folder (copied to the microSD)
entry = "index.php"        # entry file within src
```
<!-- @endcode-block -->

## Build and flash

<!-- @code-block language="bash" label="Build, flash, watch the serial output" -->
```bash
phpflash build && phpflash flash && phpflash monitor
```
<!-- @endcode-block -->

Because this is a `microsd` project, `project-src/index.php` is copied to the card root at flash time. Keep a card in the board — the script both runs from it and writes to it. To iterate by hand, copy `project-src/index.php` to the card root, reinsert, and press reset.

## What you'll see

A real run on an ESP32-P4 prints the write, the read-back match and the appended line:

<!-- @code-block language="text" label="Serial output" -->
```
php-esp32: microSD mounted at /sdcard
PHP 8.3.32 on ESP32-P4
--- /sdcard/index.php ---
writing /sdcard/test.txt ...
file_put_contents returned: 27
exists: yes
size:   27
--- read back (in the same run) ---
'hello from PHP 8.3.32 tick
'
matches what we wrote: YES
--- after append ---
hello from PHP 8.3.32 tick
second line
--- end ---
```
<!-- @endcode-block -->

Pull the card afterward and `test.txt` is on it, with the appended line — the write persisted.

<!-- @callout variant="warning" title="Format the card FAT, and let writes flush before power-off" -->
The card must be formatted **FAT** (FAT32) and seated firmly, or it will not mount — check the boot log for `microSD mounted at /sdcard`. Writes are buffered by the FATFS layer and flushed when the card is unmounted at the end of the run, so let the script finish before you cut power. Yanking the card or pulling power mid-write can leave the last write unflushed or corrupt the filesystem. A card that reports I/O errors mid-run is usually a bad reseat: power off, reseat, and reset.
<!-- @endcallout -->
