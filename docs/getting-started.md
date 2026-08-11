# Getting started

This walks through taking a board from nothing to running your own PHP: installing the toolchain,
creating a project, building, flashing, and changing what runs. It uses
[`phpflash`](https://github.com/php-baremetal/flash-tool), the CLI that drives the whole thing. The
raw ESP-IDF commands are at the end for when you want them.

The mental model is small. A *project* is a config file plus a `project-src/` folder. The config picks
the board, where the code lives, how it runs, and which extensions to compile. `project-src/` holds
your PHP. Building turns the two into a firmware image; the source is either copied to a microSD card
or baked into the image, depending on the storage you chose.

## Install phpflash

phpflash is a single static binary. On Linux, grab the latest release:

```sh
BASE=https://github.com/php-baremetal/flash-tool/releases/latest/download
curl -LO "$BASE/phpflash-linux-amd64"        # or phpflash-linux-arm64
chmod +x phpflash-linux-amd64
sudo mv phpflash-linux-amd64 /usr/local/bin/phpflash
phpflash --version
```

To build from source instead, clone the flash-tool repo and run `go build -o phpflash .` (Go 1.25+).

Then install the toolchain and the firmware sources, once:

```sh
phpflash system-setup
```

This clones ESP-IDF at the version php-esp32 targets and runs its installer (which brings the
cross-compilers and a private Python environment, so there is nothing to install by hand), then clones
php-esp32 and runs `scripts/fetch-php.sh` to download and patch the PHP source. It is idempotent: run
it again later and it updates the checkouts rather than re-cloning. By default everything lands under
`~/esp`; override with `--idf-path` and `--php-esp32-path`.

## Create a project

```sh
phpflash init my-project
cd my-project
```

`init` is interactive, with a default at every step, and it reads your installed php-esp32 so it only
offers what the firmware can actually build. In order, it asks:

1. **Project name**, defaulting to the directory name.
2. **Chip family**, then **board** within it. The list is read from `boards/`, so it grows as new
   boards are added. Picking the board fixes the ESP-IDF target for you.
3. **Storage type**, `microsd` or `embedded` (see below), limited to what the board supports.
4. **Project type**, the execution model: `init-loop`, `web-server`, or `event-driven`. A board with
   no network will not offer `web-server`.
5. **Optional extensions**, a multi-select. Each one you enable then asks about its own settings (for
   example `mbstring` offers oniguruma for `mb_ereg`, `openssl` offers the full OpenSSL build and the
   TLS client). The always-on extensions are not listed; they are always there.
6. **Serial port**, which you can leave empty to autodetect at flash time.
7. **Starter**, a `hello` page or a `blink` sketch to put in `project-src/index.php`.

Pass `--yes` to accept every default and skip the prompts, or `--board esp32-s3-eth` to preselect a
board. When php-esp32 is not installed yet, the board and extension steps are skipped and you edit the
config after `system-setup`.

What lands on disk:

```
my-project/
├── php-esp32.config.toml   the project config (below)
├── .gitignore              ignores build/ and the local override
└── project-src/
    └── index.php           your entry script
```

`project-src/` is the deployable. On a `microsd` project it is exactly what you copy to the card; on
an `embedded` project it is what gets baked into the image. It stays separate from the config and the
build output so it is easy to reason about and easy to version.

### The config file

`init` writes `php-esp32.config.toml`. It is meant to be read and edited:

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

For machine-specific tweaks that should not be committed (a serial port, a local toolchain path), put
them in a `php-esp32.config.local.toml` next to it. Every command overlays that file when present, and
the scaffolded `.gitignore` already excludes it.

## Storage: where the code lives

**microSD.** The source lives on a FAT32 card, with the entry script at the path in `[php] entry`. The
board reads it at boot. To change what runs, pull the card, edit the files from your PC, put it back,
and reset. No rebuild. This is the fastest loop for developing a script, and the only practical option
for a large tree like a framework's `vendor/`.

**embedded.** The build packs `project-src/` into a read-only image in the board's flash, so the board
runs with no card at all. Good for a fixed appliance, or for a board you would rather not open up. The
image is read-only; if the code needs to write (sessions, a SQLite file, a cache), enable a microSD
alongside it with `[storage] microsd = true`, and writes go to the card while the code is served from
flash.

One thing to know about `embedded`: flashing a `microsd` project afterward does not erase that baked-in
image on its own, and the firmware prefers it over the card. phpflash handles this for you (a `microsd`
flash wipes the embedded slot), but it is worth knowing if you ever flash by hand.

## Execution model

**init-loop.** For a script that runs on its own. It can be linear, running once from top to bottom, or
follow the Arduino-style shape: define `setup()` and `loop(int $tick)`, and the firmware calls `setup`
once at boot then `loop` repeatedly, passing an incrementing tick. The loop lives in C so it has a
place to yield to the watchdog and do memory housekeeping between iterations; you just fill in the two
functions. Use `delay($ms)` inside `loop` to pace it without busy-waiting.

**web-server.** For a board with Ethernet. The firmware runs an HTTP server and hands each request to a
fresh PHP run, shared-nothing, the way a script runs behind Apache. Your entry script produces the
page, `$_SERVER`/`$_GET`/`$_POST`/cookies/sessions are filled per request, and static files under a
`public/` root are served directly. Point `[php] entry` at the front controller (`index.php`, or
`public/index.php` for a framework). With OPcache enabled this is what makes Laravel and Symfony
browsable on the ESP32-P4.

**event-driven** is reserved for future use.

## Build, flash, monitor

```sh
phpflash build      # compile the firmware
phpflash flash      # build if needed, then write the board
phpflash monitor    # open the serial console (Ctrl-] to leave)
```

`build` turns the enabled extensions into a deterministic set of build flags, runs any fetch scripts
they need (oniguruma, SQLite, OpenSSL), and drives ESP-IDF into a build tree under the project's own
`build/`. Because that tree and its `sdkconfig` are per project, several projects can share one
php-esp32 install with isolated, side-by-side builds, and a build does not depend on whatever a shared
directory held before.

`flash` checks that the connected chip matches the project's board before writing, and refuses with a
clear message if you have, say, an S3 project but a P4 plugged in (pass `--force` to override). It
picks the port from `-p`, then `[board].port`, then the first serial device it finds.

`monitor` opens the console. On a networked board the boot log prints the address it came up on
(`network up -- http://<ip>/`), which is where you point a browser or `curl` for a `web-server`
project.

## Changing what runs

On a `microsd` project: pull the card, edit `index.php`, put it back, reset. If you changed code that
OPcache has already cached, clear the cache folder on the card too (and Symfony's `var/cache`) so the
board recompiles.

On an `embedded` project: edit `project-src/`, then `phpflash flash` to rebuild the image.

## Identify a board

If you are not sure what is plugged in, or you have a blank board fresh from the shop:

```sh
phpflash discover          # identify the chip and list the boards that match it
phpflash discover --all    # actively probe a blank board's peripherals to name it
```

Plain `discover` reads the chip with esptool and lists the boards in that family, including a
ready-to-paste `[board] target` line. `--all` goes further: it flashes a small probe firmware, built
per candidate board so it uses each board's real wiring, brings up the Ethernet link and mounts the
card, and names the match. `--all` overwrites the app on the board, so it asks first and reminds you to
re-flash afterward.

## Doing it by hand with ESP-IDF

phpflash is a front end over ESP-IDF; you can drive it directly when you want to.

Load the environment in each shell (the state does not persist between commands), and fetch the PHP
source once:

```sh
source ~/esp/esp-idf/export.sh
./scripts/fetch-php.sh        # downloads, verifies sha256, applies patches
```

Build, selecting the board and PHP version (`./scripts/info.sh` lists what is available). The target
comes from the board, so there is no `set-target`:

```sh
idf.py -DBOARD=esp32-s3-eth -DPHP_VERSION=8.4.24 build
```

Optional extensions are off by default; turn them on with their flags (`-DPHP_EXT_MBSTRING=ON`, and so
on; see [ext-porting.md](ext-porting.md)). For an embedded project add `-DPHP_EMBED_SRC=<dir>` to bake
the source in. If you switch board or version in place, remove the stale `sdkconfig` first
(`rm sdkconfig`) so it regenerates.

Flash and read the output over the same cable:

```sh
idf.py -p /dev/ttyACM0 flash monitor
```

Reset and download mode are automatic; no buttons to hold. Leave the monitor with `Ctrl-]`, and reset
the board without reflashing with `Ctrl-T` `Ctrl-R`.

## Troubleshooting

**No serial port.** Check `ls /dev/ttyACM* /dev/ttyUSB*`. If nothing appears, the USB cable must carry
data, not just power.

**Permission denied or port busy.** Add yourself to the `dialout` group and log back in
(`sudo usermod -aG dialout $USER`), or unlock the port for now with `sudo chmod a+rw /dev/ttyACM0`. On
Fedora a busy port is often `ModemManager` probing the device; stop or mask it.

**The board keeps resetting** with a backtrace in the monitor: `idf.py monitor` decodes the symbols
already. Out of memory during a request usually means the app needs more PSRAM than the board has (a
full framework does not fit the ESP32-S3's 8 MB, for instance).

**The microSD will not mount.** It must be FAT32 and seated firmly; the boot log should show
`microSD mounted at /sdcard`. A card that reports I/O errors mid-run is usually a bad reseat: power the
board off, reseat it, and reset.

**`idf.py: command not found`.** You have not loaded the environment in this shell
(`source ~/esp/esp-idf/export.sh`). phpflash does this for you.
