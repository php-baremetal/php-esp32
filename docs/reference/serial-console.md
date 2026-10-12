---
eyebrow: 'Docs · Reference'
lede:    'How to read what your PHP prints over the serial console: the one command, where the output actually goes (it depends on how the board wires its USB), and the traps -- the wrong port on a two-USB board, native-USB reset into download mode, and Linux permissions.'
see_also:
  - { href: '../getting-started/quick-start.md', meta: 'Getting started', label: 'Quick start' }
  - { href: './special-boards.md', meta: 'Reference', label: 'Special boards' }
  - { href: 'https://github.com/php-baremetal/flash-tool', meta: 'external', label: 'phpflash' }
prev: { label: 'Special boards', href: './special-boards.md' }
next: { label: 'No next page', href: '#' }
---

# Reading the serial console

Everything your PHP prints -- `echo`, `print`, `printf`, `var_dump`, an uncaught exception's trace --
comes out of the board's **serial console**, together with the firmware's own boot log. There is no
screen on most boards, so the serial console is how you see that the thing is alive and what it is
doing. The console runs at **115200 baud**.

## The one command

From the project directory:

<!-- @code-block language="bash" label="open the monitor" -->
```bash
phpflash monitor        # opens the serial monitor; exit with Ctrl-]
```
<!-- @endcode-block -->

That is all it takes when the port is found correctly. The rest of this page is about the times it
isn't -- because the output can be on a port you didn't expect, or silenced by a board that reset
itself into the wrong mode.

## Where the output goes depends on the board

The console is not always on the same USB connector. It follows how the board wires its USB-C, which
the board's preset records with its `console` key:

| `console` | The console comes out on | Boards |
| --------- | ------------------------ | ------ |
| `usb-jtag` | the chip's **native USB** (USB-Serial/JTAG, built into the SoC) | ESP32-S3-Zero, ESP32-S3-ETH, ESP32-S3-Touch |
| `uart` *(or unset)* | **UART0**, routed to an on-board USB-to-UART **bridge** (CP2102 / CH340) | ESP32-S3-DevKitC-1, every ESP32-P4 board |

This is set for you by the preset; you don't choose it per run. It only decides *which USB endpoint*
carries `echo` and the logs. See the `console` key in the board's preset file.

On Linux the two endpoints show up as different device nodes:

- a **native USB** (USB-Serial/JTAG) endpoint is `/dev/ttyACM*`
- a **UART bridge** is `/dev/ttyUSB*`

## Trap 1 -- a board with two USB ports (DevKitC-1)

The **ESP32-S3-DevKitC-1** has *two* USB-C sockets, and they are not interchangeable:

- the one silk-screened **UART** is the CP2102 bridge -- this carries the console (`/dev/ttyUSB0`);
- the one silk-screened **USB** is the chip's native USB -- flashing and JTAG, **no console**
  (`/dev/ttyACM0`).

With this board's `uart` console, **plug into the UART port** to read PHP's output. Nothing comes out
of the USB port.

The catch: `phpflash monitor` with no `-p` autodetects, and it tries **`/dev/ttyACM*` first**, then
`/dev/ttyUSB*`. If both DevKitC sockets are plugged in, autodetect grabs the native-USB `ttyACM` one --
the port *without* the console -- and you see a dead, silent monitor even though the board is running
fine. Name the port explicitly, or pin it in the config:

<!-- @code-block language="bash" label="force the UART-bridge port" -->
```bash
phpflash monitor -p /dev/ttyUSB0
```
<!-- @endcode-block -->

<!-- @code-block language="toml" label="php-baremetal.config.toml" -->
```toml
[esp32]
port = "/dev/ttyUSB0"      # pin it once; flash and monitor both use it
```
<!-- @endcode-block -->

> Flashing and the console can live on different ports. On the DevKitC you can flash over either
> socket and read the console from the UART one -- plug both in and pin `port` to the UART bridge.

## Trap 2 -- native-USB boards reset into download mode

On a `usb-jtag` board (S3-Zero, S3-ETH, S3-Touch) the console shares the **one** native-USB port with
flashing. That single port is convenient, but it has a sharp edge: a **reset** issued over it can land
the chip in **download (bootloader) mode** instead of running your app. In download mode the app never
starts, so the console stays silent -- the board looks dead, but it is only waiting to be flashed.

Two things help:

- **Just watch a running app, don't reset it.** The interactive `phpflash monitor` attaches without
  forcing the app to restart. The non-interactive read *does* reset by default (to catch the boot log);
  turn that off to tail a running app:

<!-- @code-block language="bash" label="tail without resetting" -->
```bash
phpflash monitor --reset=false -d 20      # read 20s of a running app, no reset
```
<!-- @endcode-block -->

- **If it's stuck in download mode, power-cycle it.** A clean power cycle (unplug/replug, or the board's
  reset without holding BOOT) boots the app. A software reset through the USB-Serial/JTAG is exactly the
  thing that tends to drop back into download, so reach for the power cycle, not another reset.

This is the reason a freshly flashed native-USB board sometimes prints nothing until you physically
unplug and replug it.

## Choosing the port by hand

Autodetect is "first `/dev/ttyACM*`, then `/dev/ttyUSB*`, lowest-numbered first." That is right for a
single native-USB board and wrong whenever a bridge board, or a second board, is also plugged in. When
in doubt, list what's there and pick:

<!-- @code-block language="bash" label="see what's connected" -->
```bash
ls /dev/ttyUSB* /dev/ttyACM*          # the serial nodes present right now
phpflash discover                     # identify the connected board (chip + preset)
phpflash monitor -p /dev/ttyUSB0      # then monitor the one you mean
```
<!-- @endcode-block -->

A reliable habit on a multi-board bench is to pin `[esp32].port` per project, so neither flash nor
monitor ever guesses.

## Trap 3 -- permission denied on Linux

The serial node belongs to the `dialout` group, and a fresh user account usually isn't in it, so the
first `monitor` or `flash` fails with a permission error. Add yourself once and re-log (or start a new
session):

<!-- @code-block language="bash" label="grant serial access (once)" -->
```bash
sudo usermod -aG dialout "$USER"      # then log out and back in
```
<!-- @endcode-block -->

`phpflash` checks access without opening the device (opening would toggle the reset lines), so this
shows up as a clear up-front error rather than a silent hang.

## Non-interactive reads (logs, CI, scripts)

For capturing output without sitting in an interactive monitor:

<!-- @code-block language="bash" label="scripted reads" -->
```bash
phpflash monitor -d 15                       # read for 15 seconds, then exit
phpflash monitor -u "ready" -d 30            # read until the line "ready", capped at 30s
phpflash monitor --reset=false -d 10         # tail a running app (no reset) for 10s
```
<!-- @endcode-block -->

`-u/--until` stops as soon as the marker appears; `-d/--duration` caps the wait (default 30s) so it
can't hang forever. These default to **resetting first** to catch the boot log -- the same reset that
trap 2 warns about on native-USB boards, hence `--reset=false` to tail instead.

## If the output is garbled

Unreadable bytes almost always mean a **baud mismatch** -- the console is 115200. Any other serial
tool must match it:

<!-- @code-block language="bash" label="without phpflash" -->
```bash
idf.py -p /dev/ttyUSB0 monitor        # ESP-IDF's monitor, from the php-esp32 dir
screen /dev/ttyUSB0 115200            # minimal; exit with Ctrl-A K
```
<!-- @endcode-block -->

If a native-USB board resets when you merely open the port with a generic tool, keep the hang-up bit
off so opening doesn't toggle reset:

<!-- @code-block language="bash" label="open without resetting (generic tools)" -->
```bash
stty -F /dev/ttyACM0 115200 -hupcl
cat /dev/ttyACM0
```
<!-- @endcode-block -->

## Quick checklist

- Board is `usb-jtag` (S3-Zero/ETH/Touch) -> one native-USB port, `/dev/ttyACM*`.
- Board is `uart` (DevKitC, all P4) -> read from the **UART** port / bridge, `/dev/ttyUSB*`.
- DevKitC with both sockets plugged -> autodetect picks the wrong one; pass `-p /dev/ttyUSB0`.
- Silent after flashing a native-USB board -> power-cycle (it's likely in download mode).
- Permission denied -> `usermod -aG dialout`, re-log.
- Garbage on screen -> wrong baud; it's 115200.
