---
eyebrow: 'Docs · Getting started'
lede:    'Take an ESP32 board from nothing to running your own PHP: install the toolchain and phpflash, scaffold a project, pick a storage and execution model, then build, flash, and monitor.'
see_also:
  - { href: './architecture.md', meta: '8 min' }
  - { href: '../extensions/porting-status.md', meta: '6 min' }
  - { href: '../storage/persistent-store.md', meta: '4 min' }
  - { href: 'https://github.com/php-baremetal/flash-tool', meta: 'external', label: 'phpflash' }
prev: { label: 'Overview', href: '../overview.md' }
next: { label: 'Architecture', href: './architecture.md' }
---

# Quick start

This page walks a board from nothing to running your own PHP: installing the toolchain, scaffolding a project, building, flashing, and changing what runs. It uses [`phpflash`](https://github.com/php-baremetal/flash-tool), the single-binary CLI that drives the whole thing. The raw ESP-IDF commands are at the end, for when you want them.

The mental model is small. A *project* is a config file plus a `project-src/` folder. The config picks the board, where the code lives, how it runs, and which extensions to compile. `project-src/` holds your PHP. Building turns the two into a firmware image; the source is either copied to a microSD card or baked into the image, depending on the storage you chose.

## What you need

- A supported board. Two chip families are built today: **ESP32-P4** (P4-Zero, P4-Pico, P4-ETH; RISC-V, up to 32 MB PSRAM) and **ESP32-S3** (S3-Zero, S3-Pico, S3-ETH; Xtensa LX7, 8 MB PSRAM). Within each family, `-ETH` adds a wired network, `-Pico` drops it, and `-Zero` is the minimal embedded-only variant with no microSD slot.
- A USB cable that carries **data**, not just power.
- Linux (amd64 or arm64). The commands below are Linux; the phpflash repo covers other platforms.
- Optionally, a FAT32 microSD card if you want the pull-card-and-edit development loop.

<!-- @callout variant="info" title="Two properties decide if a chip qualifies" -->
The runtime heap is measured in megabytes, so the chip needs external **PSRAM**; and the firmware image is around 3 MB, so it needs **8 MB or more of flash**. Core architecture and clock speed change how fast it runs, not whether it runs. Parts without PSRAM (the C and H series) are out regardless of clock.
<!-- @endcallout -->

## Install phpflash

phpflash is a single static binary. On Linux, grab the latest release:

<!-- @code-block language="bash" label="terminal — install phpflash" -->
```bash
BASE=https://github.com/php-baremetal/flash-tool/releases/latest/download
curl -LO "$BASE/phpflash-linux-amd64"        # or phpflash-linux-arm64
chmod +x phpflash-linux-amd64
sudo mv phpflash-linux-amd64 /usr/local/bin/phpflash
phpflash --version
```
<!-- @endcode-block -->

To build from source instead, clone the flash-tool repo and run `go build -o phpflash .` (Go 1.25+).

## Install the toolchain and firmware sources

Then install the cross-toolchain and the firmware sources, once:

<!-- @code-block language="bash" label="terminal — system-setup" -->
```bash
phpflash system-setup
```
<!-- @endcode-block -->

This clones ESP-IDF at the version php-esp32 targets and runs its installer (which brings the cross-compilers and a private Python environment, so there is nothing to install by hand), then clones php-esp32 and runs `scripts/fetch-php.sh` to download and patch the PHP source. It is idempotent: run it again later and it updates the checkouts rather than re-cloning. By default everything lands under `~/esp`; override with `--idf-path` and `--php-esp32-path`.

<!-- @callout variant="note" title="What gets fetched" -->
The PHP source is not committed (around 210 MB, reproducible from the official tarball). `fetch-php.sh` downloads it, verifies its sha256, and applies the target patches. Optional extension sources (oniguruma, SQLite, OpenSSL) are fetched on demand at build time, only when you enable the extension that needs them.
<!-- @endcallout -->

## The install-to-running walkthrough

<!-- @steps -->
- **Install phpflash** — drop the single binary on your `PATH` (see above) and confirm `phpflash --version` prints.
- **Run `system-setup`** — `phpflash system-setup` installs ESP-IDF and the php-esp32 firmware sources under `~/esp`. Do this once per machine.
- **Scaffold a project** — `phpflash init my-project` asks a short series of questions (board, storage, execution model, extensions) and writes the config plus a starter `project-src/index.php`. Then `cd my-project`.
- **Write your PHP** — edit `project-src/index.php` (or point `[php] entry` at a front controller like `public/index.php` for a framework).
- **Build** — `phpflash build` turns the enabled extensions into build flags and drives ESP-IDF into the project's own `build/` tree.
- **Flash** — `phpflash flash` builds if needed, checks the connected chip matches the project's board, and writes it.
- **Monitor** — `phpflash monitor` opens the serial console. Watch the boot log; leave with `Ctrl-]`.
<!-- @endsteps -->

## Create a project

<!-- @code-block language="bash" label="terminal — init" -->
```bash
phpflash init my-project
cd my-project
```
<!-- @endcode-block -->

`init` is interactive, with a default at every step, and it reads your installed php-esp32 so it only offers what the firmware can actually build. In order, it asks:

<!-- @steps -->
- **Project name** — defaults to the directory name.
- **Chip family, then board** — the list is read from `boards/`, so it grows as new boards are added. Picking the board fixes the ESP-IDF target for you.
- **Storage type** — `microsd` or `embedded`, limited to what the board supports (a `-Zero` board offers embedded only).
- **Project type** — the execution model: `init-loop`, `web-server`, or `event-driven`. A board with no network will not offer `web-server`.
- **Optional extensions** — a multi-select. Each one you enable then asks about its own settings (for example `mbstring` offers oniguruma for `mb_ereg`, `openssl` offers the full OpenSSL build and the TLS client). The always-on extensions are not listed; they are always there.
- **Serial port** — leave it empty to autodetect at flash time.
- **Starter** — a `hello` page or a `blink` sketch to put in `project-src/index.php`.
<!-- @endsteps -->

Pass `--yes` to accept every default and skip the prompts, or `--board esp32-s3-eth` to preselect a board. When php-esp32 is not installed yet, the board and extension steps are skipped and you edit the config after `system-setup`.

<!-- @callout variant="tip" title="Boards only offer real modes" -->
A board's `board.toml` declares the storage and project types its hardware supports, but a mode is offered only when it is *both* listed there *and* marked implemented in the selected PHP version's manifest. That is why the P4-Pico, with no wired network, omits `web-server` while the P4-ETH and S3-ETH include it.
<!-- @endcallout -->

## Project layout

What lands on disk:

<!-- @code-block language="text" label="tree — project layout" -->
```text
my-project/
├── php-esp32.config.toml   the project config (below)
├── .gitignore              ignores build/ and the local override
└── project-src/
    └── index.php           your entry script
```
<!-- @endcode-block -->

`project-src/` is the deployable. On a `microsd` project it is exactly what you copy to the card; on an `embedded` project it is what gets baked into the image. It stays separate from the config and the build output so it is easy to reason about and easy to version.

### The config file

`init` writes `php-esp32.config.toml`. It is meant to be read and edited:

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
name         = "my-project"
storage_type = "microsd"       # where the PHP source lives: microsd | embedded
type         = "init-loop"     # execution model: init-loop | web-server | event-driven

[board]
target = "esp32-p4-pico"       # the board; its family decides the ESP-IDF target
port   = ""                    # serial port; empty = autodetect at flash time

[php]
src   = "project-src"          # the PHP source folder
entry = "index.php"            # entry file within it (a framework sets e.g. public/index.php)

# One table per enabled extension: `enabled`, plus any settings it declares.
[extensions.mbstring]
enabled = true
onig    = true                 # build mb_ereg (bundles oniguruma)
```
<!-- @endcode-block -->

The scaffold also leaves `[esp-idf]` and `[php-esp32]` tables (each with `path` and `version`) so a project can pin a specific toolchain or firmware checkout; leave them empty to use the `system-setup` defaults.

<!-- @callout variant="tip" title="Keep machine-specific settings out of git" -->
For tweaks that should not be committed (a serial port, a local toolchain path), put them in a `php-esp32.config.local.toml` next to the main file. Every command overlays that file when present, and the scaffolded `.gitignore` already excludes it.
<!-- @endcallout -->

## Storage: where the code lives

The two storage types are a per-project choice, and they change the flash partition layout: an embedded image is sized to its source, a microSD project carries none of it and leaves that flash free.

<!-- @tabs labels="microSD, embedded" -->
<!-- @tab index="0" -->
The source lives on a FAT32 card, with the entry script at the path in `[php] entry`. The board reads it at boot. To change what runs, pull the card, edit the files from your PC, put it back, and reset. No rebuild.

This is the fastest loop for developing a script, and the only practical option for a large tree like a framework's `vendor/`. The boot log confirms the card with `microSD mounted at /sdcard`.
<!-- @endtab -->
<!-- @tab index="1" -->
The build packs `project-src/` into a read-only image in the board's flash, so the board runs with no card at all. Good for a fixed appliance, or for a board you would rather not open up.

The image is read-only; if the code needs to write (sessions, a SQLite file, a cache), enable a microSD alongside it with `[storage] microsd = true`, and writes go to the card while the code is served from flash.
<!-- @endtab -->
<!-- @endtabs -->

<!-- @callout variant="warning" title="Embedded images survive a microSD flash" -->
Flashing a `microsd` project afterward does not erase a baked-in image on its own, and the firmware prefers the embedded copy over the card. phpflash handles this for you — a `microsd` flash wipes the embedded slot — but it is worth knowing if you ever flash by hand.
<!-- @endcallout -->

## Execution models

The three execution models are chosen per project with the `type` field.

### init-loop — a script that runs on its own

An `init-loop` project can be **linear**, running once from top to bottom and stopping, or follow the **Arduino-style** shape: define `setup()` and `loop(int $tick)`, and the firmware calls `setup` once at boot then `loop` repeatedly, passing an incrementing tick. The loop lives in C so it has a place to yield to the watchdog and do memory housekeeping between iterations; you just fill in the two functions. Use `delay($ms)` inside `loop` to pace it without busy-waiting.

The simplest linear script:

<!-- @code-block language="php" label="project-src/index.php — linear" -->
```php
<?php
echo "Hello from PHP " . PHP_VERSION . " on an ESP32-P4!\n";
echo "2 ** 16 = " . (2 ** 16) . "\n";
echo "memory in use: " . memory_get_usage() . " bytes\n";

foreach (['world', 'chip', 'microcontroller'] as $who) {
    echo "hello, $who\n";
}
```
<!-- @endcode-block -->

The same model, Arduino-style, driving a physical pin:

<!-- @code-block language="php" label="project-src/index.php — setup/loop" -->
```php
<?php
// setup() runs once; loop($tick) repeats. The C side owns the loop
// (watchdog + memory housekeeping); PHP provides these two functions.
define('LED', 2);

function setup(): void {
    gpio_mode(LED, GPIO_OUTPUT);
    echo "setup: blinking an LED on GPIO " . LED . " from PHP " . PHP_VERSION . "\n";
}

function loop(int $tick): void {
    gpio_write(LED, $tick % 2);   // on for odd ticks, off for even
    if ($tick % 10 === 0) {
        echo "tick $tick\n";
    }
    delay(500);                    // milliseconds
}
```
<!-- @endcode-block -->

`gpio_mode`, `gpio_write`, `gpio_read` and `delay` come from a small built-in extension. `delay` yields the core through `vTaskDelay` instead of busy-waiting, so the watchdog stays satisfied, and `echo` goes to the serial console. A project can add its own native functions the same way, by dropping a C extension under `./firmware/exts/` — see [custom extensions](../extensions/custom-extensions.md).

<!-- @callout variant="danger" title="Sizing an LED for a 3.3 V pin" -->
The GPIO drives 3.3 V. A red LED with a ~330 ohm resistor is a safe bet; a blue or white LED (higher forward voltage) can be too dim to see at 3.3 V. Wire the LED and resistor between the pin and GND.
<!-- @endcallout -->

### web-server — HTTP in front, PHP per request

For a board with Ethernet. The firmware runs an HTTP server and hands each request to a fresh PHP run, shared-nothing, the way a script runs behind Apache or PHP-FPM. Your entry script produces the page, `$_SERVER`/`$_GET`/`$_POST`/cookies/sessions are filled per request, and static files under a `public/` root are served directly. Point `[php] entry` at the front controller (`index.php`, or `public/index.php` for a framework). With OPcache enabled this is what makes stock Laravel and Symfony browsable on the ESP32-P4.

<!-- @code-block language="php" label="project-src/index.php — web-server" -->
```php
<?php
// Runs fresh for every HTTP request, behind the firmware's HTTP server.
// Whatever you echo becomes the response body.
$uri    = $_SERVER['REQUEST_URI']    ?? '/';
$method = $_SERVER['REQUEST_METHOD'] ?? 'GET';

echo "<!doctype html>\n";
echo "<html><head><meta charset=\"utf-8\"><title>PHP on ESP32-P4</title></head>\n";
echo "<body><h1>Hello from PHP " . PHP_VERSION . "</h1>\n";
echo "<p>you requested: <code>" . htmlspecialchars("$method $uri") . "</code></p>\n";
// Each request is a clean run, so a random number proves it re-executed.
echo "<p>fresh random number: " . random_int(1000, 9999) . "</p>\n";
echo "</body></html>\n";
```
<!-- @endcode-block -->

<!-- @callout variant="note" title="event-driven" -->
The `event-driven` project type is reserved for future use.
<!-- @endcallout -->

## Build, flash, monitor

<!-- @code-block language="bash" label="terminal — build/flash/monitor" -->
```bash
phpflash build      # compile the firmware
phpflash flash      # build if needed, then write the board
phpflash monitor    # open the serial console (Ctrl-] to leave)
```
<!-- @endcode-block -->

`build` turns the enabled extensions into a deterministic set of build flags, runs any fetch scripts they need (oniguruma, SQLite, OpenSSL), and drives ESP-IDF into a build tree under the project's own `build/`. Because that tree and its `sdkconfig` are per project, several projects can share one php-esp32 install with isolated, side-by-side builds, and a build does not depend on whatever a shared directory held before.

`flash` checks that the connected chip matches the project's board before writing, and refuses with a clear message if you have, say, an S3 project but a P4 plugged in. It picks the port from `-p`, then `[board].port`, then the first serial device it finds.

`monitor` opens the console. On a networked board the boot log prints the address it came up on (`network up -- http://<ip>/`), which is where you point a browser or `curl` for a `web-server` project.

<!-- @callout variant="warning" title="Chip-vs-board mismatch on flash" -->
`flash` cross-checks the connected chip against the project's board family and refuses to write on a mismatch. Pass `--force` to override only when you are sure, for instance when two boards in the same family differ in a way the probe cannot see.
<!-- @endcallout -->

<!-- @callout variant="note" title="Per-project sdkconfig" -->
The build tree and its generated `sdkconfig` live under the project's own `build/`, so projects sharing one php-esp32 install never fight over a shared config. If you drive ESP-IDF by hand and switch board or version in place, remove the stale `sdkconfig` first so it regenerates.
<!-- @endcallout -->

## Changing what runs

<!-- @tabs labels="microSD, embedded" -->
<!-- @tab index="0" -->
Pull the card, edit `index.php`, put it back, reset. No rebuild. If you changed code that OPcache has already cached, clear the cache folder on the card too (and Symfony's `var/cache`) so the board recompiles.
<!-- @endtab -->
<!-- @tab index="1" -->
Edit `project-src/`, then `phpflash flash` to rebuild the image and write it. The baked-in copy is read-only, so there is no faster loop than a reflash.
<!-- @endtab -->
<!-- @endtabs -->

## Identify a board

If you are not sure what is plugged in, or you have a blank board fresh from the shop:

<!-- @code-block language="bash" label="terminal — discover" -->
```bash
phpflash discover          # identify the chip and list the boards that match it
phpflash discover --all    # actively probe a blank board's peripherals to name it
```
<!-- @endcode-block -->

Plain `discover` reads the chip with esptool and lists the boards in that family, including a ready-to-paste `[board] target` line. `--all` goes further: it flashes a small probe firmware, built per candidate board so it uses each board's real wiring, brings up the Ethernet link and mounts the card, and names the match.

<!-- @callout variant="warning" title="--all overwrites the app" -->
`discover --all` flashes a probe firmware over whatever is on the board, so it asks first and reminds you to re-flash your project afterward.
<!-- @endcallout -->

## phpflash command reference

| Command | What it does |
|---|---|
| `phpflash system-setup` | Installs ESP-IDF and the php-esp32 firmware sources (once per machine). Idempotent; `--idf-path` / `--php-esp32-path` relocate. |
| `phpflash init <name>` | Scaffolds a project interactively. `--yes` accepts defaults, `--board <id>` preselects a board. |
| `phpflash build` | Compiles the firmware into the project's own `build/`. |
| `phpflash flash` | Builds if needed, checks the chip matches the board, then writes it. `-p <port>` and `--force` available. |
| `phpflash monitor` | Opens the serial console. `Ctrl-]` to leave. |
| `phpflash discover` | Identifies the chip and lists matching boards. `--all` probes a blank board's peripherals to name it. |

## Doing it by hand with ESP-IDF

phpflash is a front end over ESP-IDF; you can drive it directly when you want to.

Load the environment in each shell (the state does not persist between commands), and fetch the PHP source once:

<!-- @code-block language="bash" label="terminal — load env + fetch PHP" -->
```bash
source ~/esp/esp-idf/export.sh
./scripts/fetch-php.sh        # downloads, verifies sha256, applies patches
```
<!-- @endcode-block -->

Build, selecting the board and PHP version (`./scripts/info.sh` lists what is available). The target comes from the board, so there is no `set-target`:

<!-- @code-block language="bash" label="terminal — idf.py build" -->
```bash
idf.py -DBOARD=esp32-s3-eth -DPHP_VERSION=8.4.25 build
```
<!-- @endcode-block -->

Optional extensions are off by default; turn them on with their flags (`-DPHP_EXT_MBSTRING=ON`, and so on; see the [porting status](../extensions/porting-status.md)). For an embedded project add `-DPHP_EMBED_SRC=<dir>` to bake the source in. If you switch board or version in place, remove the stale `sdkconfig` first (`rm sdkconfig`) so it regenerates.

Flash and read the output over the same cable:

<!-- @code-block language="bash" label="terminal — idf.py flash monitor" -->
```bash
idf.py -p /dev/ttyACM0 flash monitor
```
<!-- @endcode-block -->

Reset and download mode are automatic; no buttons to hold. Leave the monitor with `Ctrl-]`, and reset the board without reflashing with `Ctrl-T` `Ctrl-R`.

## Troubleshooting

<!-- @callout variant="warning" title="No serial port" -->
Check `ls /dev/ttyACM* /dev/ttyUSB*`. If nothing appears, the USB cable must carry data, not just power.
<!-- @endcallout -->

<!-- @callout variant="warning" title="Permission denied or port busy" -->
Add yourself to the `dialout` group and log back in (`sudo usermod -aG dialout $USER`), or unlock the port for now with `sudo chmod a+rw /dev/ttyACM0`. On Fedora a busy port is often `ModemManager` probing the device; stop or mask it.
<!-- @endcallout -->

<!-- @callout variant="warning" title="The board keeps resetting" -->
A backtrace in the monitor is decoded by `idf.py monitor` already. Out of memory during a request usually means the app needs more PSRAM than the board has — a full framework does not fit the ESP32-S3's 8 MB, for instance.
<!-- @endcallout -->

<!-- @callout variant="warning" title="The microSD will not mount" -->
It must be FAT32 and seated firmly; the boot log should show `microSD mounted at /sdcard`. A card that reports I/O errors mid-run is usually a bad reseat: power the board off, reseat it, and reset.
<!-- @endcallout -->

<!-- @callout variant="warning" title="idf.py: command not found" -->
You have not loaded the environment in this shell (`source ~/esp/esp-idf/export.sh`). phpflash does this for you.
<!-- @endcallout -->

## Next steps

- Read how the memory map and execution flow fit together in [architecture](./architecture.md).
- See which extensions are built-in, optional, or not ported in the [porting status](../extensions/porting-status.md).
- Persist values across reboots with the [persistent store](../storage/persistent-store.md), or share data across one boot's requests with the [in-RAM store](../storage/in-ram-store.md).
- Bake endpoints and device names into the firmware as `$_ENV` with a project [`.env`](../storage/environment.md).
