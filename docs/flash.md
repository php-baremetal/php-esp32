# Build, flash, read the output

## The quick way

Two scripts do everything:

```bash
./setup.sh      # system prerequisites + ESP-IDF + PHP source (once)
./flash.sh      # builds, asks for the port, flashes, then offers the monitor
./monitor.sh    # just opens the serial monitor (no build, no flash)
```

The rest of this file explains the same steps by hand, plus troubleshooting.

## Environment

You need ESP-IDF v5.5.x (I use 5.5.5). It brings its own `riscv32-esp-elf` toolchain and a
dedicated Python environment: no compilers to install by hand. `idf.py` isn't on the PATH
by default, so load it in every shell (the state doesn't persist between commands):

```bash
source ~/esp/esp-idf/export.sh
```

The first time you also need the PHP source, which isn't committed:

```bash
./scripts/fetch-php.sh    # downloads it, checks its sha256 and applies the local patches
```

## Build

```bash
idf.py set-target esp32p4     # first time only
idf.py build
```

## Connecting

The board's USB-C port is an onboard USB-UART bridge (CH343P) wired to the chip's UART0,
with automatic reset and download lines. A single USB-C cable carries power, flashing and
console: no external serial adapter and no wiring on the header needed.

Plug in USB-C and find the serial port:

```bash
ls /dev/ttyACM* /dev/ttyUSB*
```

On Linux it's usually `/dev/ttyACM0` (depending on the kernel it may be `/dev/ttyUSB0`). If
nothing shows up: the cable must be a data cable, not charge-only.

Permissions: if you get `permission denied` or `port is busy`, add yourself to the
`dialout` group and log back in, or unlock the port on the fly:

```bash
sudo usermod -aG dialout $USER    # permanent, after a logout/login
sudo chmod a+rw /dev/ttyACM0      # on the fly, resets when the board is replugged
```

If the port stays busy on Fedora, it's usually `ModemManager` probing the CDC devices:
`sudo systemctl stop ModemManager` (or `mask` it if you don't use a modem).

## Flash and monitor

The same cable and port both write the app and read its output:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Reset and entering download mode are automatic, no need to hold buttons down. To leave the
monitor: `Ctrl-]`. Handy: `Ctrl-T` `Ctrl-R` to reset the board without reflashing.

## Changing what runs

No rebuild. Pull out the microSD, rewrite `index.php` from your computer (`examples/` has
several ready-made examples, each in its own folder with an `index.php` to copy), put the
card back in the slot, press reset. The board rereads the file at boot.

The microSD must be formatted FAT32, with `index.php` in the root.

## When something goes wrong

The board keeps resetting (backtrace in the monitor): `idf.py monitor` already decodes the
symbols; alternatively `riscv32-esp-elf-addr2line -e build/php-esp32.elf <address>`.

The microSD won't mount: check that it's FAT32 and seated properly. The log must show
`microSD mounted at /sdcard`.

`idf.py: command not found`: you haven't loaded the environment in this shell
(`source ~/esp/esp-idf/export.sh`).

## Handy commands

```bash
idf.py build                  # build
idf.py -p PORT flash          # write the app
idf.py -p PORT monitor        # console (Ctrl-] to exit)
idf.py -p PORT flash monitor  # flash + monitor in sequence
idf.py fullclean              # wipe build/ completely
```
