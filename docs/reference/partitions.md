---
eyebrow: 'Docs · Reference'
lede:    'How the firmware lays out flash: a partition table generated per build from the board''s fixed spec plus an auto-sized storage image, what a board definition pins (flash size and — critically — the PSRAM mode), and why small, cheap boards are the tight-fit case to watch.'
see_also:
  - { href: './footprint.md', meta: 'Reference', label: 'Footprint' }
  - { href: '../storage/persistent-store.md', meta: 'Storage', label: 'Persistent store' }
  - { href: 'https://github.com/php-baremetal/flash-tool', meta: 'external', label: 'phpflash (project-side partitions)' }
prev: { label: 'Porting notes', href: './porting-notes.md' }
next: { label: 'No next page', href: '#' }
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
| **Fixed** | `nvs`, `phy_init`, `factory` (the app) | the board's committed `boards/<family>/<board>/partitions.csv` |
| **Generated** | `storage` (embedded source FAT), `phpstore` (persistent `store_*` NVS) | sized and appended per build |

- `storage` is added only for an **embedded** project, sized to the PHP source (cluster-accurate
  occupancy + FAT overhead + `[storage] reserve_kb`). A microSD project has no `storage` partition —
  the source runs from the card and `main.c` falls back to it.
- `phpstore` is added only when the project sets `[store] size_kb` (see [Persistent store](../storage/persistent-store.md)).

The generated CSV lands in `build/partitions.gen.csv` (never committed) and is selected via a
sdkconfig fragment layered last, so it wins over the board's committed default.

## What a board definition pins

A board is a directory under `boards/<family>/<board>/`. Two files decide the flash layout:

<!-- @code-block language="ini" label="sdkconfig.board — the parts that matter here" -->
```ini
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y      # how much flash the table must fit inside
CONFIG_ESPTOOLPY_FLASHSIZE="4MB"
CONFIG_SPIRAM_MODE_QUAD=y             # PSRAM electrical mode (Quad vs Octal) -- see below
```
<!-- @endcode-block -->

- **`partitions.csv`** — the fixed partitions, above all the `factory` size that caps how big the
  firmware can be (and, on a small board, how much is left for `storage`).
- **`sdkconfig.board`** — layered last over the base and the family (`boards/<family>/sdkconfig.family`),
  so board choices win. It pins the **flash size** and, when the board differs from its family, the
  **PSRAM mode**.

<!-- @callout variant="warning" title="PSRAM mode is not cosmetic — it decides whether PHP runs at all" -->
The build sets `USE_ZEND_ALLOC=0`, so the entire Zend runtime heap lives in PSRAM. If PSRAM does not
initialise, the engine has no memory and the board never gets past boot. PSRAM comes in **Quad** and
**Octal** electrical modes, and a module only speaks one. The ESP32-S3-ETH/Pico carry Octal PSRAM (the
family default); the tiny ESP32-S3-Zero carries **Quad**. Pinning the wrong mode is a silent failure —
`phpflash discover` reads the real chip so you can set the right one.
<!-- @endcallout -->

## Small-flash boards: the tight-fit case

The board in the photo — an ESP32-S3-Zero (ESP32-S3FH4R2) — has **4 MB flash and 2 MB Quad PSRAM**,
far less than the S3-ETH's 16 MB / 8 MB Octal. Its board definition overrides the family assumptions:

| Setting | Family default (S3-ETH) | ESP32-S3-Zero |
|---|---|---|
| Flash | 16 MB | **4 MB** |
| PSRAM | Octal, 8 MB | **Quad, 2 MB** |
| `factory` | 10 MB | **3456K** (app is ~2.8 MB; leaves room for `storage`) |

With those three lines the ~2.8 MB firmware, a small embedded `storage` FAT, and `nvs`/`phy_init` all
fit inside 4 MB, and 2 MB of PSRAM is enough for a small script. (A framework the size of Symfony needs
far more heap than 2 MB, so it belongs on a big-PSRAM board — this is a memory limit, not a bug.)

## Bringing up or fixing a board

When you meet a board whose flash or PSRAM differs from what its definition assumes, don't guess:

<!-- @steps -->
- **Read the real chip** `phpflash discover` prints the true flash size and PSRAM (e.g.
  `Embedded Flash 4MB (XMC), Embedded PSRAM 2MB (AP_3v3)`).
- **Pin flash size and PSRAM mode** in `sdkconfig.board` (`CONFIG_ESPTOOLPY_FLASHSIZE*`, and
  `CONFIG_SPIRAM_MODE_QUAD`/`_OCT` when it differs from the family).
- **Size `factory`** in the board's `partitions.csv` so the firmware fits and leaves room for the
  generated partitions on that flash.
<!-- @endsteps -->

The `esp32-s3-zero` board is exactly this correction applied — a good template for a new small board.

## Overriding it per project

The fixed table belongs to the board, but a single project can take it over without forking the
firmware: drop a `partitions.csv` next to `php-esp32.config.toml` and the build uses it instead (the
generated `storage`/`phpstore` are still appended). `phpflash partitions publish` writes a starter
table from the board's defaults, with a per-board list of sensible `factory` sizes — see the phpflash
docs. The [`custom-partitions`](https://github.com/php-baremetal/php-esp32/tree/master/examples/custom-partitions) example shows it end to end.
