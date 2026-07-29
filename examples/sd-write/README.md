# sd-write

A diagnostic: can PHP **write** a file to the microSD and read it back? Every other example
only ever reads from the card (the script, `require`d files, a Composer `vendor/` tree), so
this is the first check that the write path works end to end.

No extension and no wiring needed — `file_put_contents` / `file_get_contents` are core.

## What it does

Writes a short string to `/sdcard/test.txt`, reads it back in the same run and compares, then
appends a second line (a second open/write) and prints the result.

```php
$path = '/sdcard/test.txt';
$data = "hello from PHP " . PHP_VERSION . " tick\n";
$n = file_put_contents($path, $data);
$back = file_get_contents($path);
echo $back === $data ? "match\n" : "MISMATCH\n";
file_put_contents($path, "second line\n", FILE_APPEND);
```

## What it demonstrates

- **PHP writes to the SD, not just reads.** `file_put_contents` opens, writes and closes; the
  FATFS mount is read-write and is flushed when the card is unmounted at the end of the run.
- **Reads see the write, and it persists.** The read-back matches, `filesize()` is right, and
  `test.txt` is on the card (with the appended line) when you put it back in a computer.

## Output

From a real run on the board:

```
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
```

## Running it

Copy `index.php` to the microSD as `/index.php` (any firmware is fine), put the card back,
press reset. Read the serial output, and optionally check that `test.txt` is on the card
afterwards.
