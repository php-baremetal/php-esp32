# custom-partitions

A project that ships **its own partition table**, on a deliberately tight board: the **ESP32-S3-Zero**
(ESP32-S3FH4R2 — 4 MB flash, 2 MB PSRAM). It shows the two things you reach for when the board's
default layout does not fit your project, or you want a different split.

## Why this exists

The partition table has two layers:

- **Fixed partitions** (`nvs`, `phy_init`, `factory` = the app) come from the **board**
  (`boards/<family>/<board>/partitions.csv`). The `factory` size is what caps how big the firmware
  can be — and on a 4 MB board it also has to leave room for everything else.
- **Generated partitions** — the embedded source FAT (`storage`) and the persistent store
  (`phpstore`) — are sized and appended **per build**, so you never write them by hand.

On the 4 MB S3-Zero the ~2.8 MB firmware, the embedded source, and the store all have to fit in one
small flash. This project shrinks `factory` from the board default (3456K) to **3200K** to leave a
little more room, and turns on the persistent store.

## The custom table

`partitions.csv` here was produced with:

```
phpflash partitions publish
```

That drops the board's table into the project with guidance comments; it was then edited (the
`factory` line). Because a `partitions.csv` sits next to the config, `phpflash build` uses it
automatically — you'll see `==> using project partition table: ./partitions.csv`. Delete the file to
fall back to the board default. Full guide: `docs/recipes/custom-partition-table.md` (flash-tool docs).

## What it does

`index.php` prints the PHP version and free heap, then increments a **reboot-persistent boot
counter** with `store_*`. The counter lives in the `phpstore` NVS partition — one of the partitions
appended automatically next to our custom `factory`, enabled by `[store] size_kb = 32` in the config.

## Build & flash

```
phpflash build
phpflash flash
phpflash monitor
```

The build's `check_sizes` step confirms the app fits the custom `factory`, and the boot log shows the
generated table (`factory` 3200K + `storage` + `phpstore`). Reset the board a few times: the boot
count goes up and survives power cycles.

## Adapting it

- Different board? Change `[board] target` and re-run `phpflash partitions publish` — the stub always
  matches the selected board (and its flash size).
- Need more room for the embedded source instead? Grow it with `[storage] reserve_kb` rather than
  editing the table.
- The board's **flash size** and **PSRAM mode** are board-level (`sdkconfig.board`), not project-level;
  the S3-Zero, for instance, needs Quad PSRAM. See the custom-partition-table recipe for the full
  picture.
