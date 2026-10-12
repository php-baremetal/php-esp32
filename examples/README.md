# Examples

Two starter examples live here, next to the engine — the same two `phpflash init` offers as starters.
The full example catalog (language, extensions, sensors, display, event-driven, network, frameworks,
power, security) lives in its own repository:

**→ <https://github.com/php-baremetal/examples>**

## The two starters

| Folder | What it does | Needs |
|---|---|---|
| [`hello/`](hello/) | A linear script: prints and stops. The bare minimum to see the engine alive. | nothing |
| [`led-blink/`](led-blink/) | The setup/loop model: blinks an LED forever. | LED + ~330 ohm between GPIO2 and GND |

Each is a self-contained project: a `php-baremetal.config.toml` selecting the board, storage mode and
any extensions, plus a `project-src/` with the PHP. To try one:

```sh
cd hello
phpflash build      # build the firmware
phpflash flash      # flash it to the board
phpflash monitor    # watch the serial output
```

To run from a microSD instead of flashing the source in, copy the contents of `project-src/` to the
card root (so its `index.php` sits at `/index.php`), insert the card and reset the board.

## Everything else

The rest of the examples moved to <https://github.com/php-baremetal/examples>, grouped by topic. Clone
it alongside your `php-esp32` checkout and build any example with the same `phpflash build` / `flash` /
`monitor` flow — `phpflash` finds this engine via the config, `PHP_ESP32_DIR`, or its default.
