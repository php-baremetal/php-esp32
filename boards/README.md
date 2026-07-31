# Boards

Each board is `boards/<family>/<board>/` — a chip **family**, then a specific **board** on it.
Pick one with `-DBOARD=<board>` (or `<family>/<board>`); the default is `default_board` in the
repo-root [`php-esp32.toml`](../php-esp32.toml). `./scripts/info.sh` lists what's available.

## What a board owns

- `board.toml` — identity + which storage/execution modes its **hardware** supports (read by
  flash-tool; intersected with what the firmware implements, in the version manifest).
- `board.h` / `board.c` — the board's code behind the common interface
  (`board_mount_storage()` / `board_unmount_storage()`), plus `BOARD_NAME` / `BOARD_SOC`.
- `board.cmake` — its sources, include dir and ESP-IDF driver requirements.
- `sdkconfig.board` — board-level config (flash size, chip revision, console, partition filename).
- `partitions.csv` — its flash layout.

## What a family owns (`boards/<family>/`)

- `family.toml` — chip identity (name, ESP-IDF target, PSRAM).
- `sdkconfig.family` — chip-level config (target, PSRAM), shared by every board of the family.

`sdkconfig` is layered **base** (`../sdkconfig.defaults`) → **family** → **board** (most
specific wins), assembled by the top-level `CMakeLists.txt`.

## Add a board

1. `mkdir boards/<family>/<newboard>/` and add `board.h`, `board.c`, `board.cmake`,
   `sdkconfig.board`, `partitions.csv`, `board.toml` — copy an existing board and adapt the
   pins, the mount code, the flash/console config, and the supported storage/project types.
2. Build with `idf.py -DBOARD=<newboard> ...` (or set it as `default_board`). **Changing board
   needs `rm sdkconfig` first** — defaults don't merge into an existing `sdkconfig`.
3. `./scripts/check-manifest.py` validates the new `board.toml`.

## Add a chip family

`mkdir boards/<newfamily>/` with `family.toml` + `sdkconfig.family` (its ESP-IDF target and
PSRAM), then add boards under it as above.
