# Boards

Each board is `boards/<family>/<board>/`: a chip **family**, then a specific **board** on it. Pick one
with `-DBOARD=<board>` (or `<family>/<board>`); the default is `default_board` in the repo-root
[`php-esp32.toml`](../php-esp32.toml). With phpflash you never pass it by hand, `init` sets it from the
board you choose. `./scripts/info.sh` lists what the checkout can build.

## Boards in this repo

| Board | Family | Notes |
|---|---|---|
| `esp32-p4-pico` | `esp32-p4` | Waveshare ESP32-P4-Pico. 4-bit SDMMC microSD, 32 MB flash, no network. The default board. |
| `esp32-p4-eth` | `esp32-p4` | Waveshare ESP32-P4-ETH. The same P4 SD design as the Pico (plus a GPIO45 card-power switch) and a wired Ethernet PHY (IP101GRI over RMII), so it runs the `web-server` model. |
| `esp32-s3-eth` | `esp32-s3` | Waveshare ESP32-S3-ETH. Xtensa LX7, 8 MB PSRAM, 16 MB flash. microSD over SPI and a W5500 Ethernet controller over SPI. Runs plain apps and a live web server. |

Two families are supported today, `esp32-p4` (RISC-V) and `esp32-s3` (Xtensa). Others in the ESP32
line with PSRAM and enough flash are candidates, and a new one is a directory here, not a change to
the engine.

## What a board owns

- `board.toml`: identity, and which storage and execution modes its **hardware** supports (read by
  phpflash and intersected with what the firmware implements, from the version manifest).
- `board.h` and `board.c`: the board's code behind the common interface (`board_mount_storage()`,
  `board_unmount_storage()`, and `board_network_up()` if it has a network), plus the `BOARD_NAME`,
  `BOARD_SOC` and optional `BOARD_HAS_NETWORK` macros. `main.c` talks only to this interface, so a
  board can present an SDMMC card or an SPI card, an internal Ethernet MAC or an SPI W5500, and the
  rest of the firmware does not know the difference.
- `board.cmake`: its sources, include dir and ESP-IDF driver requirements.
- `sdkconfig.board`: board-level config (flash size, chip revision, console, partition filename).
- `partitions.csv`: its flash layout.

## What a family owns

Under `boards/<family>/`:

- `family.toml`: chip identity (name, ESP-IDF target, PSRAM).
- `sdkconfig.family`: chip-level config (the ESP-IDF target and the PSRAM setup), shared by every
  board in the family.

The `sdkconfig` is layered base (`../sdkconfig.defaults`), then family, then board, most specific
wins, assembled by the top-level `CMakeLists.txt`. That top level also pins the ESP-IDF target from the
family's `sdkconfig.family` before the build starts, so a stray `sdkconfig` from an earlier build of a
different family cannot make ESP-IDF guess the wrong architecture.

## Add a board

1. `mkdir boards/<family>/<newboard>/` and add `board.h`, `board.c`, `board.cmake`, `sdkconfig.board`,
   `partitions.csv` and `board.toml`. Copy an existing board and adapt the pins, the mount and network
   code, the flash and console config, and the supported storage and project types. Pick the closest
   starting point: a P4 board for SDMMC and an internal MAC, the S3-ETH for SPI storage and an SPI
   Ethernet controller.
2. Build with phpflash (set the board in the project config) or `idf.py -DBOARD=<newboard>`. Changing
   the board in place needs `rm sdkconfig` first, since defaults do not merge into an existing one.
3. `./scripts/check-manifest.py` validates the new `board.toml`.

## Add a chip family

`mkdir boards/<newfamily>/` with `family.toml` and `sdkconfig.family` (its ESP-IDF target and PSRAM
setup), then add boards under it as above. Install the target's toolchain first
(`idf_tools.py install --targets=<target>`); PHP itself needs no changes, since the portable VM builds
on any target ESP-IDF supports.
