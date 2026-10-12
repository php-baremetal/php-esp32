---
eyebrow: 'Docs · Reference'
lede:    'How the firmware lays out flash: a partition table generated per build from the board''s fixed spec plus an auto-sized storage image, what a board definition pins (flash size and — critically — the PSRAM mode), and why small, cheap boards are the tight-fit case to watch.'
see_also:
  - { href: './footprint.md', meta: 'Reference', label: 'Footprint' }
  - { href: '../storage/persistent-store.md', meta: 'Storage', label: 'Persistent store' }
  - { href: 'https://github.com/php-baremetal/flash-tool', meta: 'external', label: 'phpflash (project-side partitions)' }
prev: { label: 'Porting notes', href: './porting-notes.md' }
next: { label: 'Special boards', href: './special-boards.md' }
---

# Partitions and the flash layout

The firmware image, the PHP source, and any persistent data all live in the chip's flash, carved into
partitions. php-esp32 does not ship one fixed table: it **generates** it per build, so the same source
adapts to a microSD project (source on the card) or an embedded one (source packed into flash), and to
boards with very different flash sizes.

![A fingertip-sized ESP32-S3 board — 4 MB flash and 2 MB PSRAM. Boards this small are the tight-fit case for the flash layout.](https://raw.githubusercontent.com/php-baremetal/php-esp32/master/docs/assets/tiny-esp32-s3.jpg)

*A ~$4, fingertip-sized ESP32-S3 (4 MB flash / 2 MB PSRAM). On boards this small the ~3 MB firmware,
the embedded source, and the store all have to fit at once — this page is about making that work.*

## How the table is built

`cmake/gen-partitions.cmake` runs before the build and assembles the table from two layers:

| Layer | Partitions | Source |
|---|---|---|
| **Fixed** | `nvs`, `phy_init` | the chip's committed `boards/<chip>/partitions.csv` |
| **Generated** | `factory` (the app), `storage` (embedded source FAT), `phpstore` (persistent `store_*` NVS) | sized and appended per build |

- `storage` is added only for an **embedded** project, sized to the PHP source (cluster-accurate
  occupancy + FAT overhead + `[storage] reserve_kb`). A microSD project has no `storage` partition —
  the source runs from the card and `main.c` falls back to it.
- `phpstore` is added only when the project sets `[store] size_kb` (see [Persistent store](../storage/persistent-store.md)).

The generated CSV lands in `build/partitions.gen.csv` (never committed) and is selected via a
sdkconfig fragment layered last, so it wins over the chip's committed default.

## What decides the flash layout

The flash size and the PSRAM mode come from the preset and its location, not from a per-board
sdkconfig. Two things decide the layout:

- **Flash size** — the preset's `flash` field. phpflash passes it to the build as `PHP_FLASH_SIZE_KB`,
  and `factory` is regenerated to **fill** that flash (whatever is left after `nvs`/`phy_init`,
  `storage` and `phpstore`). There is no per-board magic factory size: one layout fits whatever flash
  it is built for. If the project does not fit — the embedded source plus the store leave less than
  1 MB for the app — the build stops with a clear error naming the sizes.
- **PSRAM mode** — the quad-vs-octal leaf under the chip (`boards/esp32-s3/{quad,octal}/sdkconfig.chip`),
  chosen by where the preset lives. The P4 has a single in-package PSRAM configuration, so it has no
  mode leaf.

- **`partitions.csv`** (`boards/<chip>/partitions.csv`) holds only the fixed rows (`nvs`, `phy_init`)
  and a small fallback `factory` used only if a build ever omits the flash size. `factory`, `storage`
  and `phpstore` are regenerated per build.

<!-- @callout variant="warning" title="PSRAM mode is not cosmetic — it decides whether PHP runs at all" -->
The build sets `USE_ZEND_ALLOC=0`, so the entire Zend runtime heap lives in PSRAM. If PSRAM does not
initialise, the engine has no memory and the board never gets past boot. PSRAM comes in **Quad** and
**Octal** electrical modes, and a module only speaks one. The octal S3 boards (ESP32-S3-ETH, the N8R8
DevKitC) carry Octal PSRAM; the tiny ESP32-S3-Zero carries **Quad**. A quad preset lives under
`quad/`, an octal one under `octal/`, so selecting the preset selects the right mode —
`phpflash discover` reads the real chip to confirm it.
<!-- @endcallout -->

## Small-flash boards: the tight-fit case

The board in the photo — an ESP32-S3-Zero (ESP32-S3FH4R2) — has **4 MB flash and 2 MB Quad PSRAM**,
far less than the octal S3's 16 MB / 8 MB. Its preset lives under `quad/presets/` and sets
`flash = "4MB"`:

| Setting | Octal S3 (ESP32-S3-ETH) | ESP32-S3-Zero |
|---|---|---|
| Flash | 16 MB | **4 MB** |
| PSRAM | Octal, 8 MB | **Quad, 2 MB** |
| `factory` | fills 16 MB | **fills 4 MB** (app ~2.8 MB; the rest is left for `storage`) |

The ~2.8 MB firmware, a small embedded `storage` FAT, and `nvs`/`phy_init` all fit inside 4 MB, and
2 MB of PSRAM is enough for a small script. (A framework the size of Symfony needs far more heap than
2 MB, so it belongs on a big-PSRAM board — this is a memory limit, not a bug.)

## Bringing up a new board

When you bring up a board whose flash or PSRAM you are unsure of, don't guess:

<!-- @steps -->
- **Read the real chip** `phpflash discover` prints the true flash size and PSRAM (e.g.
  `Embedded Flash 4MB (XMC), Embedded PSRAM 2MB (AP_3v3)`).
- **Set the preset's `flash`** to that size, and place the preset under the matching PSRAM-mode leaf
  (`quad/` or `octal/`) so the right `sdkconfig.chip` applies.
- **Nothing else to size** — `factory` is regenerated to fill the flash, and the build stops with a
  clear error if the project does not fit.
<!-- @endsteps -->

The `waveshare-esp32-s3-zero` preset is exactly this — a good template for a new small board.

## Overriding it per project

The fixed table belongs to the board, but a single project can take it over without forking the
firmware: drop a `partitions.csv` next to `php-baremetal.config.toml` and the build uses it instead (the
generated `storage`/`phpstore` are still appended). `phpflash partitions publish` writes a starter
table from the board's defaults, with a per-board list of sensible `factory` sizes — see the phpflash
docs. The [`custom-partitions`](https://github.com/php-baremetal/php-esp32/tree/master/examples/custom-partitions) example shows it end to end.
