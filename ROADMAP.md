# Roadmap

How I want to structure and carry `php-esp32` forward. Apart from the **1.0.0 milestone** below, which
has a target date, this is **direction, not dates**: the project is built in spare time, so release
cycles are longer than they were, and items move between sections as things land or priorities shift.
It is a living document — open an issue or a discussion if you think something is missing or mis-ordered.

## 1.0.0 — target: mid-September 2026

The first stable release. It's a consolidation milestone, not a big pile of new features: get the
foundations solid enough to call the project 1.0.

- **Complete the test CI.** The full GitHub Actions pipeline finished and green: manifest checks plus
  base-firmware builds across every version, board and project type, with caching and the per-board
  image-size budget in place.
- **Generalise and refine the existing extensions.** A pass over the extensions already built
  (`gpio`, `wifi`, `openssl`, `mbstring`, `sqlite`, `s3_onboard_rgb`, `baremetal_utility`): consistent
  APIs and naming, tighter option handling, and shared patterns so they're easy to learn and to build
  on — rather than adding brand-new ones.
- **`ext/sqlite3` support.** Ship the SQLite3-class API alongside PDO, selectable per project with
  `[extensions.sqlite] type = "pdo-sqlite" | "sqlite3"`. *(Landed in 0.19.0; it rolls up into 1.0.0.)*
- **Launch the blog on the website.** Turn on the blog on the project site and make it the **official
  channel** for announcements, release notes, deep-dives — and sneak peeks of what's coming. If you
  want to keep up with the project or catch the spoilers, that's where to look.

## 2.0.0 — the big capabilities — target: end of October 2026

The release where the platform grows new axes. These are larger pieces of work, taken on after the
1.0.0 foundations are solid.

- **Bluetooth from PHP.** A `bluetooth` extension exposing the radio the way `wifi` does — BLE first
  (advertise, scan, GATT), driven straight from ESP-IDF, opt-in per project.
- **Displays.** First-class display support beyond the current SSD1306 examples: a general driver
  layer for common panels (I²C/SPI OLED and TFT), so a script can draw to a screen without
  hand-writing the C.
- **Touch.** Touch input from PHP — both the SoC's capacitive touch pads and touch controllers on
  display panels — so a script can read taps and gestures, pairing with the display work above.
- **IMU boards.** Support for boards carrying an IMU (accelerometer / gyroscope), with the sensor
  readable from PHP, so motion-sensing projects work out of the box.
- **The `event-driven` execution model.** The third project type (today reserved): a script reacts to
  events — GPIO interrupts, timers, network, incoming BLE — instead of a linear loop or a per-request
  server. The C side owns the event loop and dispatches into PHP handlers.
- **Proper multi-core support (ESP32-S3 and ESP32-P4).** Both families are dual-core, but PHP runs
  single-threaded today. Make real use of the second core — offloading the C side (httpd, drivers,
  the event loop) and defining a safe, well-specified boundary between it and the single PHP engine.
- **A first real project, not just an example.** A complete, non-trivial application that ties the
  capabilities together — network, a display with touch, a sensor (IMU), persistent storage, all
  driven from PHP — so people can see what the platform actually does end to end, beyond the
  single-feature examples. It doubles as the reference for how to structure a real project.

## 3.0.0 — beyond ESP32: going multi-family — target: end of March 2027

The **goal milestone**: the release where "PHP on a microcontroller" stops meaning "PHP on an ESP32".
Reaching it is the point where the project can be considered firmly on track — a stable, multi-family
platform rather than a single-chip experiment. It's a structural shift, so it's the furthest-out
milestone and the one most open to change along the way.

- **A second chip family: NXP.** Bring the port to an NXP family in its own dedicated repository,
  proving the engine and the build model aren't tied to ESP-IDF or to Espressif silicon.
- **A generic, family-agnostic project config.** Retire `php-esp32.config.toml` in favour of a
  configuration that isn't ESP32-specific — one format that describes a project's board, storage,
  execution model and extensions across families, so the same project definition works whichever chip
  it targets.
- **Extensions out of the main repo, into a versioned registry.** The custom extensions move out of
  the firmware tree into a dedicated, versioned extension repository. The tooling then resolves and
  downloads on the fly only the extensions a project actually asks for, and builds them in — think
  Composer, but for bare-metal PHP extensions. This keeps the core small and lets extensions evolve
  and be shared on their own schedule.
- **A new tool, most likely from scratch.** `phpflash` today is best treated as a convenience for the
  current ESP32-only layout. A multi-family world (generic config + an extension registry) will most
  likely mean tearing it down and building a fresh tool around the new model, rather than stretching
  the current one to fit.

## Contributing

New contributors are very welcome, and this is a good time to jump in.

- **Good first areas:** a new board profile, a peripheral extension, an example, or documentation.
  These are self-contained and don't require touching the engine port.
- **Before large work,** open an issue or a discussion so we can agree on the shape — especially for
  anything touching the build, the manifest, or the version patches.
- The existing extensions (`components/php_ext_*`), the per-project C extensions
  (`examples/*/firmware/exts/`) and the board profiles (`boards/`) are the patterns to copy.

If you're using this for something, telling me about it genuinely helps steer the roadmap.
