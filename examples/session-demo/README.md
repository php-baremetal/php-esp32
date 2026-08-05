# session-demo

`ext/session` on the microcontroller: `session_start()`, `$_SESSION`, and the default **files** save
handler persisting to a writable directory — here a folder on the **microSD**. The session survives
reboots (the visit counter keeps climbing), and with the `web-server` project type the same
mechanism carries a session across HTTP requests.

> Needs a **writable filesystem** — a microSD. This example uses embedded source with the card
> mounted alongside (`[storage] microsd = true`) so the session files land on `/sdcard`.

## What it does

```php
ini_set('session.save_path', '/sdcard/sessions');
ini_set('session.use_cookies', '0');   // no HTTP here -- set the id ourselves
session_id('demo');
session_start();
$_SESSION['visits'] = ($_SESSION['visits'] ?? 0) + 1;
session_write_close();                  // written to /sdcard/sessions/sess_demo
```

Enable it with `[extensions.session] enabled = true` (~50 KB).

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

## The output

From [`monitor.txt`](monitor.txt). Reset the board and the count keeps climbing — it's on the card:

```
session 'demo': visit #6 (persisted to /sdcard/sessions)
read back -> visits=6, note="hello from PHP on ESP32"
session file: present on the card
```

## Notes

- **Sessions need a writable path.** The microSD is the writable filesystem here; the embedded
  source image is read-only. Point `session.save_path` at a folder on `/sdcard`.
- **No cookies.** With no HTTP layer, the example sets `session.use_cookies = 0` and drives the
  session id itself (`session_id('demo')`). Under the `web-server` project type, PHP handles the
  cookie per request as usual.
- The files handler works on the FAT filesystem; `FD_CLOEXEC` (meaningless on this target) is
  skipped by a small port patch so `session_start()` stays quiet.
