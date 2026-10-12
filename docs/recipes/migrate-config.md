---
eyebrow: 'Docs · Recipes'
lede: 'Bring a project from an older release forward: rename the config, move the board block under [esp32], and swap the old board key for a preset.'
see_also:
  - href: ../../boards/README.md
    meta: 'Boards'
    label: 'Chips, modes and presets'
  - href: ../getting-started/quick-start.md
    meta: 'Getting started'
    label: 'Quick start (phpflash init)'
---

# Migrate an older project

A board is described as a **chip** plus a **preset** (a named product), and the project config lives in `php-baremetal.config.toml`. A project from an older release carries a `php-esp32.config.toml` with a `[board]` section and a board key like `esp32-p4-pico`; those keys no longer resolve. This recipe brings such a project forward.

The fastest path is to run `phpflash init` in a fresh folder, answer the prompts, and copy your `project-src/` over the generated one. The steps below are the manual equivalent — do them when you want to keep the existing folder.

## 1. Rename the config file

<!-- @code-block language="bash" label="rename the project config" -->
```bash
git mv php-esp32.config.toml php-baremetal.config.toml   # or plain mv
```
<!-- @endcode-block -->

## 2. Move the board block under `[esp32]`

The portable keys (`name`, `type`, `[php]`, `[extensions.*]`, `[store]`, ...) stay exactly where they are. Only the board and toolchain blocks move:

<!-- @code-block language="toml" label="before — php-esp32.config.toml" -->
```toml
[board]
target = "esp32-p4-pico"
port   = ""

[esp-idf]
path    = ""
version = ""

[php-esp32]
path    = ""
version = ""
```
<!-- @endcode-block -->

<!-- @code-block language="toml" label="after — php-baremetal.config.toml" -->
```toml
[esp32]
preset = "waveshare-esp32-p4-pico"
port   = ""

[esp32.idf]
path    = ""
version = ""

[esp32.sdk]
path    = ""
version = ""
```
<!-- @endcode-block -->

- `[board]` becomes `[esp32]`, and `target = "<board>"` becomes `preset = "<product>"` (table below).
- `[esp-idf]` becomes `[esp32.idf]`, and `[php-esp32]` becomes `[esp32.sdk]`.
- `storage_type` (and any `[storage]` / `[network]` blocks) move under `[esp32]` too.

## 3. Map the old board key to a preset

| Old `target` | New `[esp32]` |
|---|---|
| `esp32-p4-pico` | `preset = "waveshare-esp32-p4-pico"` |
| `esp32-p4-eth` | `preset = "waveshare-esp32-p4-eth"` |
| `esp32-p4-wifi-c6` | `preset = "waveshare-esp32-p4-wifi6"` |
| `esp32-p4-zero` | `family = "esp32-p4"` + `storage_type = "embedded"` (generic P4, no SD/network) |
| `esp32-s3-zero` | `preset = "waveshare-esp32-s3-zero"` |
| `esp32-s3-eth` | `preset = "waveshare-esp32-s3-eth"` |
| `esp32-s3-pico` | `family = "esp32-s3/octal"` + your microSD pins (no 1:1 product preset) |

If your exact board is not listed, pick the closest preset, or select the chip/mode by `family` and declare the peripherals yourself — see [boards/README.md](../../boards/README.md). `phpflash discover` reads the real chip so you can confirm the flash size and PSRAM mode.

## 4. Raw `idf.py` builds

With phpflash you never pass `-DBOARD` — the preset in the config drives it. If you drive ESP-IDF by hand, `-DBOARD=<board-key>` no longer resolves; pass the chip, or the chip and PSRAM mode:

| Old | New |
|---|---|
| `-DBOARD=esp32-p4-pico` | `-DBOARD=esp32-p4` |
| `-DBOARD=esp32-s3-eth` | `-DBOARD=esp32-s3/octal` |
| `-DBOARD=esp32-s3-zero` | `-DBOARD=esp32-s3/quad` |

## 5. Rebuild from clean

The old `sdkconfig` does not merge into the new layering, so remove it first:

<!-- @code-block language="bash" label="rebuild" -->
```bash
rm -f sdkconfig
phpflash build --clean
```
<!-- @endcode-block -->

The build fails fast with a clear message if a board key still points at the old model, or if the project no longer fits the board's flash.
