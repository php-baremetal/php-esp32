# Boards

A board is described as **chip → (PSRAM mode) → preset**:

- a **chip** directory holds one config-driven `board.c` for every board of that chip, plus its
  ESP-IDF target and peripheral capabilities;
- a **PSRAM mode** leaf exists only where the mode is an irreducible build-time split (the S3's quad
  vs octal). A chip with a single PSRAM configuration (the P4) has no mode leaf;
- a **preset** is a thin TOML naming a real product and its flash size, console and peripheral pins.

Select a preset with `[esp32] preset = "<name>"`, or select a chip/mode by hand with
`[esp32] family = "esp32-s3/octal"` (or `"esp32-p4"`), which uses that mode's `generic` preset.
A raw build can pass `-DBOARD=esp32-s3/octal` or `-DBOARD=esp32-p4`. The default is `default_board`
in the repo-root [`php-esp32.toml`](../php-esp32.toml). With phpflash you rarely pass a board by
hand. `./scripts/info.sh` lists what the checkout can build.

## Why PSRAM mode is a leaf

The number of modes per chip equals the number of irreducible PSRAM-mode variants. The quad-vs-octal
split on the S3 **must** be fixed in the bootloader (4 vs 8 data lines, electrically incompatible, no
runtime fallback); with `USE_ZEND_ALLOC=0` the whole PHP heap lives in PSRAM, so the wrong mode means
PHP never starts. Everything else — PSRAM size, flash size, peripherals — is adaptive or config-driven,
not baked into the board identity. The P4 has a single in-package PSRAM configuration, so it has no mode
leaf: its presets sit directly under `presets/`.

## ESP32-S3 presets

`boards/esp32-s3/` is the chip (one `board.c` for every S3 board); `quad/` and `octal/` are the PSRAM
modes; `presets/` under each names a product. Flash size is adaptive (the preset's, or the mode default)
and guarded at flash time, so it is not part of the build identity.

| Preset | Mode | Flash | Peripherals |
|---|---|---|---|
| `waveshare-esp32-s3-zero` | quad | 4 MB | embedded; WS2812 on GPIO21 (via the `led` extension); USB-JTAG |
| `espressif-esp32-s3-devkitc-1-n8r2` | quad | 8 MB | embedded; onboard LED via the `led` extension |
| `espressif-esp32-s3-devkitc-1-n8r8` | octal | 8 MB | embedded; onboard LED via the `led` extension |
| `espressif-esp32-s3-devkitc-1-n16r8` | octal | 16 MB | = the N8R8 build, larger flash |
| `waveshare-esp32-s3-zero-n8r8` | octal | 8 MB | embedded; WS2812 on GPIO21; USB-JTAG |
| `waveshare-esp32-s3-eth` | octal | 16 MB | microSD (SPI) + W5500 Ethernet (SPI); USB-JTAG |
| `waveshare-esp32-s3-touch-lcd-1.85b` | octal | 16 MB | microSD (SPI); round ST77916 display, touch and sensors on I2C; USB-JTAG |

Each mode also has a `generic` preset (no peripherals preset) for selecting the mode by hand.

## ESP32-P4 presets

`boards/esp32-p4/` is the chip (one `board.c`, a single 32 MB in-package PSRAM configuration, no mode
leaf); `presets/` names a product directly.

| Preset | Flash | Peripherals |
|---|---|---|
| `waveshare-esp32-p4-pico` | 32 MB | microSD (4-bit SDMMC/SDIO); no network; CH343P UART console. The default product. |
| `waveshare-esp32-p4-eth` | 32 MB | microSD (SDIO) + wired Ethernet (internal EMAC + IP101 RMII PHY); runs `web-server`. |
| `waveshare-esp32-p4-wifi6` | 32 MB | microSD (SDIO) + WiFi 6 via an on-board ESP32-C6 companion (ESP-HOSTED). A **special board** — see [docs/reference/special-boards.md](../docs/reference/special-boards.md). |
| `generic` | 32 MB | no peripherals preset; the project adds its own, validated against the chip. |

## What a chip owns

- `chip.toml`: the ESP-IDF target and the peripheral **capabilities** (which microSD modes and network
  types the chip can drive). phpflash validates a preset's / project's peripherals against these.
- `board.c` / `board.h` / `board.cmake`: one config-driven implementation behind the common interface
  (`board_mount_storage()`, `board_unmount_storage()`, `board_network_up()`). Each peripheral's code is
  compiled only when the project/preset enables it, and its pins come from `-D` defines (falling back to
  the common module wiring); the `board.cmake` REQUIRES follow the same flags. `main.c` talks only to
  the interface, so it does not know whether a card is on SPI or SDMMC, or a net on a W5500 or a MAC.
- `sdkconfig.chip`: the chip-level config shared by every board of that chip.
- `partitions.csv`: the fixed partitions (nvs, phy); `factory` and `storage` are regenerated per build.
- Per PSRAM mode (S3): `<mode>/sdkconfig.chip`, `<mode>/meta.toml` (`default_flash`), `<mode>/presets/*.toml`.
  A mode-less chip (P4) carries `meta.toml` and `presets/` directly.

A preset is a thin TOML: `name`, optional `flash`/`console`, and the pins of the peripherals that board
has from the factory (`[storage.microsd]`, `[network]`). Its chip and mode are derived from its location.

The `sdkconfig` is layered base (`../sdkconfig.defaults`) → chip (`sdkconfig.chip`) → PSRAM mode
(`sdkconfig.chip`, where present) → preset fragment (written by phpflash), most specific wins, assembled
by the top-level `CMakeLists.txt`. A raw `idf.py -DBOARD=esp32-s3/<mode>` (or `-DBOARD=esp32-p4`) build
with no preset falls back to the mode's `default_flash`.

## Add a board

- **A new product:** add `boards/<chip>/<mode>/presets/<vendor-model>.toml` (or
  `boards/<chip>/presets/<vendor-model>.toml` on a mode-less chip) with its flash, console and
  peripheral pins. No code — the chip's `board.c` already handles the peripherals it supports.
- **A new chip:** `mkdir boards/<chip>/` with `chip.toml`, `sdkconfig.chip`, `board.c`/`board.h`/
  `board.cmake`, `partitions.csv`, and a `<mode>/` leaf per irreducible PSRAM mode (`sdkconfig.chip`,
  `meta.toml`, `presets/`) — or `meta.toml` + `presets/` directly if the chip has a single PSRAM
  configuration. Install the target's toolchain first (`idf_tools.py install --targets=<target>`); PHP
  itself needs no changes, since the portable VM builds on any target ESP-IDF supports.

Changing the board in place needs `rm sdkconfig` first, since defaults do not merge into an existing one.
`./scripts/check-manifest.py` keeps the version manifest (the flash-tool contract) in sync with the build.
