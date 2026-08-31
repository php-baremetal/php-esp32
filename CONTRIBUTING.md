# Contributing to php-esp32

Thanks for being here. This project runs the real PHP engine on microcontrollers, and it's built in
spare time — contributions are genuinely welcome, and there's a lot of self-contained work that doesn't
require touching the engine port. If you're using it for something, telling me about it also helps
steer the [roadmap](ROADMAP.md).

## Ways to contribute

Good first areas, roughly easiest first — each is self-contained:

- **An example.** A small project under `examples/` showing one feature end to end.
- **Documentation.** Fixes, a clearer getting-started path, a how-to, or a recipe under `docs/`.
- **A board profile.** Support a board that isn't covered yet — a directory under `boards/`.
- **A PHP extension.** A per-project C extension in an example, or a built-in extension wrapping a
  peripheral or a PHP `ext/*`.
- **Bug fixes and hardening.** Especially anything found while running on real hardware.

Bigger things (the build, the manifest, the version patches, a new PHP version, a new chip family)
are very welcome too — just **open an issue or a discussion first** so we can agree on the shape before
you write a lot of code.


> **Testing on a real board is mandatory before opening any PR.** This is firmware — a change that
> compiles is not a change that works. Every PR that affects behaviour must be flashed to real
> hardware and confirmed working, and the PR must state which board(s) and PHP version(s) you tested on,
> with the relevant serial output. PRs without hardware verification will not be merged. (Docs-only or
> comment-only changes are the exception.)

## Before you start

- **Skim the [ROADMAP](ROADMAP.md)** to see where the project is heading and whether your idea fits a
  planned milestone.
- **Search open issues and discussions** so we don't duplicate work.
- **For anything non-trivial, open an issue or discussion first.** A quick "I'd like to add X, here's my
  plan" saves everyone time and avoids a PR that has to be reshaped.

## The principles the project holds to

New work should hold to these — they're what keep the project coherent:

- **Real PHP, unmodified.** The engine is the pristine release from [php.net](https://www.php.net/),
  fetched at build time and **never edited in place**. Every target-specific change is a separate
  build-time patch under `components/php/versions/<ver>/patches/`. If you think you need to edit the
  engine directly, that's a sign to open a discussion.
- **The manifest is the source of truth.** `components/php/versions/<ver>/manifest.toml` describes what
  the firmware can build; `phpflash` reads it and only ever offers real options. If you add or change a
  build flag, the manifest changes with it, and `scripts/check-manifest.py` must stay green.
- **Opt-in weight.** Extensions and features are off by default and cost only the flash they add. The
  base image stays small.
- **Verified on hardware.** A change is done when it runs on a real board, not when it compiles. Say in
  your PR which board(s) and PHP version(s) you tested on, and paste the relevant serial output.
- **Follow the surrounding code.** Match the style, naming and comment density of the files you touch.

## Building and testing

You'll need the toolchain from the [quick start](docs/getting-started/quick-start.md). The short version:

- **Build a project** with [`phpflash`](https://github.com/php-baremetal/flash-tool) (`phpflash build`,
  `flash`, `monitor`), or drive `idf.py` directly:
  `idf.py -DBOARD=<board> -DPHP_VERSION=<ver> [flags] build`.
- **Check the manifest** after any build-flag / board / extension change, for every version you touched:
  `python3 scripts/check-manifest.py <version>` (must exit 0).
- **CI** runs the manifest checks and base-firmware builds across versions, boards and project types on
  push and PR. Keep it green; if your change needs a matrix entry, add it.

There's no way around the hardware step for a functional change: flash it to a board and confirm the
behaviour before marking it done.

## How-to, by area

- **Add a board** — copy the closest existing profile under `boards/<family>/<board>/` (board.toml,
  board.h, sdkconfig.board, partitions.csv, board.cmake) and adjust. See
  [boards/README.md](boards/README.md).
- **Add / port an extension** —
  - *Per-project C extension* (project-local, the low-friction option): a directory under an example's
    `firmware/exts/<name>/` with a `<name>.c` defining `zend_module_entry <name>_module_entry`. See
    [docs/extensions/custom-extensions.md](docs/extensions/custom-extensions.md).
  - *Built-in extension* (ships in the firmware, opt-in via the manifest): a `components/php_ext_<name>/`
    component, registered in each version's `internal_functions.c` and declared in the manifest. Copy an
    existing `components/php_ext_*` as the pattern.
- **Add a PHP version** — a new `components/php/versions/<ver>/` directory (version.env with the
  sha256, patches, sources.cmake, extensions.cmake, manifest.toml). Versions coexist. See
  [components/php/versions/README.md](components/php/versions/README.md).
- **Add an example** — a folder under `examples/` with `php-esp32.config.toml`, `project-src/`, a
  `README.md`, and (once you've run it) a `monitor.txt` of the real output. Don't commit the `build/`
  directory.
- **The why behind the port** — [docs/reference/porting-notes.md](docs/reference/porting-notes.md) is
  the reference for the technical choices; skim it before touching the engine build.

## Pull requests

- **Branch** off `master`; keep a PR focused on one thing.
- **Explain** what and why in the description, and **state your hardware verification**: board(s), PHP
  version(s), and the serial output that shows it working.
- **Keep the tree clean** — `check-manifest` green, no committed `build/` artifacts, no editing the
  vendored PHP source in place (use a patch).
- **Match the docs** — if you add a feature, add or update the relevant `docs/` page and a `CHANGELOG.md`
  entry.
- Small, reviewable commits with clear messages are appreciated.

## AI assistance

Using an AI assistant to help write a contribution is fine — it's a tool. What matters is that the code
is correct, that **you understand it and stand behind it**, and that it's verified on hardware like any
other change. Please don't open PRs of unreviewed generated code you haven't run. The full policy is in
[AI_USAGE.md](AI_USAGE.md).

## Reporting bugs and security issues

- **Bugs:** open a GitHub issue with the board, the PHP version, your config/`index.php` (minimal if you
  can), and the serial output. Redact secrets first (see the log-sharing guidance in
  [SECURITY.md](SECURITY.md)).
- **Security vulnerabilities:** do **not** open a public issue — follow [SECURITY.md](SECURITY.md).
  Note that vulnerabilities in PHP proper or in ESP-IDF go to those projects, not here.

## License

By contributing, you agree that your contributions are licensed under this project's
[MIT License](LICENSE), the same as the rest of the project's own code. (The PHP source the build
fetches stays under the [PHP License](https://www.php.net/license/); it is not part of this repository.)
