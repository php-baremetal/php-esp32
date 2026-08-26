---
eyebrow: 'Docs · Storage & state'
lede:    'Ship a project .env alongside the config and phpflash bakes its variables into the firmware at build time, exposing them to PHP as $_ENV and getenv() — build-time configuration that travels in the app image, not on the card.'
see_also:
  - { href: './persistent-store.md', meta: '4 min' }
  - { href: './in-ram-store.md', meta: '3 min' }
  - { href: '../getting-started/quick-start.md', meta: '10 min' }
  - { href: '../reference/footprint.md', meta: '5 min' }
prev: { label: 'In-RAM store', href: './in-ram-store.md' }
next: { label: 'Blink an LED', href: '../recipes/blink-an-led.md' }
---

# Build-time environment (`.env`)

A project can ship a `.env` file next to its `php-esp32.config.toml`. When you build, `phpflash` reads that file, compiles its variables into the firmware image, and the running engine exposes them to PHP the ordinary way — as `$_ENV` and via `getenv()`. Nothing is hardcoded in the PHP source and nothing has to live as a file on the microSD.

<!-- @code-block language="php" label="reading baked-in values from PHP" -->
```php
$_ENV['APP_NAME'];      // "my-device"
getenv('API_BASE');     // "https://example.test"
```
<!-- @endcode-block -->

This is for the configuration a build should carry with it: a device name, an endpoint, a sample rate, a feature flag, a token. The value is fixed at build time and constant for the whole run — you change it by editing `.env` and rebuilding, not at runtime. That distinction is the whole feature, and it is what separates this from the runtime stores described below.

## What it is for

Reach for a baked `.env` when a value is *configuration of the build*, not *state of the running program*:

- A device identity or label that differs per unit or per fleet (`DEVICE_NAME=p4-lab-01`).
- An API base URL or backend endpoint the firmware talks to (`API_BASE=https://api.example.test/v1`).
- A tuning constant a build should carry (`SAMPLE_HZ=5`).
- A feature or debug flag toggled per build (`DEBUG=1`).
- A token or key you would otherwise be tempted to paste into a `.php` file or drop as a plaintext file on the card.

Because the value is compiled in, the same PHP source produces different behaviour per build without editing the code — you keep one script and vary the `.env`.

## Enabling it

The feature is on by default: if a `.env` sits beside the config, it is baked in with no configuration needed. It is controllable from `php-esp32.config.toml`.

<!-- @steps -->
1. Put a `.env` next to `php-esp32.config.toml` in the project root.
2. Add `KEY=VALUE` lines (format below).
3. Run `phpflash build` — the file is parsed and its entries are compiled into the image. `phpflash flash` writes that image to the board.
4. Read the values from PHP through `$_ENV[...]` or `getenv(...)`.
5. To change a value, edit `.env` and rebuild; the new value is compiled in on the next `phpflash build`.
<!-- @endsteps -->

With no `[env]` section in the config, the default applies — baked when `.env` exists, a silent no-op when it does not. The block only exists to override that default:

<!-- @code-block language="toml" label="php-esp32.config.toml — the [env] knobs" -->
```toml
[env]
enabled = true      # false turns baking off even when .env exists
file    = ".env"    # env file path, relative to the project
```
<!-- @endcode-block -->

`phpflash init` adds `.env` to the project `.gitignore`, so a scaffolded project keeps its environment out of version control by default.

### `[env]` keys

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `enabled` | bool | *unset* → on when the file exists | `false` disables baking even if the env file is present. An absent key means "bake when the file exists"; an absent file with `enabled` on is still a no-op. |
| `file` | string | `".env"` | Path to the env file, relative to the project directory. An absolute path is used as-is. |

Internally the `enabled` key is a pointer that distinguishes three cases: absent (`nil`) means "on when the file exists", `true` forces it on, `false` turns it off. A missing env file is never an error — the feature simply bakes nothing.

## File format

`phpflash` parses a deliberately small `.env` dialect. One `KEY=VALUE` per line. Keys are C-style identifiers matching `[A-Za-z_][A-Za-z0-9_]*`; a line whose key does not match is skipped. Everything after the first `=` on a line is the value.

<!-- @code-block language="bash" label=".env" -->
```bash
# a comment (only at the start of a line)
APP_NAME=my-device
API_BASE=https://example.test
GREETING="hello world"     # double or single quotes are stripped
export TOKEN=abc123        # a leading `export` is ignored
```
<!-- @endcode-block -->

The parsing rules, exactly as `phpflash` applies them:

- Each line is trimmed of surrounding whitespace (and a trailing `\r`, so CRLF files work).
- Blank lines and lines starting with `#` are ignored. `#` only starts a comment at the start of a line — there are no inline comments after a value.
- A single leading `export ` prefix is dropped (`export TOKEN=abc123` sets `TOKEN`).
- The split is on the **first** `=`. A line with no `=`, or with `=` as its first character (empty key), is skipped.
- The key is trimmed and must match the identifier pattern; otherwise the line is dropped.
- The value is trimmed, then unquoted: a matching pair of surrounding quotes is removed.
- Inside **double** quotes, the escapes `\n`, `\t`, `\"` and `\\` are unescaped to newline, tab, quote and backslash. **Single** quotes are literal — no escapes are processed inside them.
- There is **no shell expansion**: `${VAR}` stays the literal text `${VAR}`. There are no multiline values.

Values are always strings. `$_ENV['SAMPLE_HZ']` is the string `"5"`, not the integer `5` — cast in PHP where you need a number, and compare flags as strings.

<!-- @code-block language="php" label="values are strings — cast and compare accordingly" -->
```php
$hz    = (int) ($_ENV['SAMPLE_HZ'] ?? '1');   // "5"  -> 5
$debug = ($_ENV['DEBUG'] ?? '0') === '1';     // "1"  -> true
$name  = $_ENV['DEVICE_NAME'] ?? '(unset)';   // plain string
```
<!-- @endcode-block -->

## When the values are available

The variables are set before the PHP engine starts, so they are present for the entire run, whichever execution model the project uses:

- In an **`init-loop`** sketch, `$_ENV` and `getenv()` are populated in `setup()` and every `loop()` tick.
- In the **`web-server`** model, the values are available in the optional init script and in every request handler — they do not have to be re-read or passed around.

Change a value and rebuild; the new value is compiled in. There is no runtime API to set or mutate these from PHP — they are constants of the build.

## Where it lives, and what that means for secrets

The values are compiled into the application image in the chip's **internal flash** — not written to the microSD, and not part of your PHP source tree.

<!-- @callout variant="info" title="Baked-in is not encrypted" -->
The values are **not secret and not encrypted.** Anyone who can read the flash — physical access plus `esptool`, or an open debug port — can recover them. Treat a baked `.env` as configuration, not as a vault.

What it *does* buy you, versus keeping the same values in a plaintext file on the card:

- **Not on removable media.** A microSD can be pulled, mounted on any PC and read in seconds; the baked-in values cannot. Swapping or cloning the card does not carry them, and they never travel with it.
- **Harder to extract.** Recovering them means dumping the flash over a wire rather than reading a text file — a meaningfully higher bar for a casual attacker.

For values that must stay confidential against someone with the board in hand, use the SoC's own protections — **flash encryption** and **secure boot** — which are outside the scope of this feature.
<!-- @endcallout -->

## How it works

The pipeline is small and fully build-time:

<!-- @steps -->
1. `phpflash` reads the project's `.env` (or the `[env] file` you named) and parses it with the rules above into a list of key/value pairs.
2. It renders a small C table into `build/php_env.gen.c` — a flat array of alternating key, value string literals plus a pair count. This file is generated per build and is never committed; keys and values are C-escaped (control bytes as fixed 3-digit octal so they never merge with a following digit).
3. The generated source is handed to the firmware build via a `-DPHP_ENV_SRC=` argument. A build with no `.env` (or with `enabled = false`) bakes an empty table instead — the runtime path becomes a no-op.
4. Before the engine starts, `main.c` walks the table and applies each entry with `setenv(key, value, 1)`.
5. Because that happens *before* `php_embed_init()`, PHP's normal environment import puts the variables into `$_ENV` — the build's `variables_order` carries `E` — and `getenv()` returns them.
<!-- @endsteps -->

The generated table for a five-line `.env` looks like this:

<!-- @code-block language="text" label="build/php_env.gen.c (generated — do not edit)" -->
```text
/* Generated by phpflash from the project's .env -- do not edit. */
const char *const php_esp32_env[] = {
    "DEVICE_NAME", "p4-lab-01",
    "API_BASE", "https://api.example.test/v1",
    "SAMPLE_HZ", "5",
    "DEBUG", "1",
    "GREETING", "hello from flash",
};
const int php_esp32_env_count = 5;
```
<!-- @endcode-block -->

The application side that consumes it is equally small — a loop over the table before init:

<!-- @code-block language="text" label="main/main.c — apply_project_env()" -->
```text
extern const char *const php_esp32_env[];
extern const int php_esp32_env_count;

static void apply_project_env(void)
{
    for (int i = 0; i < php_esp32_env_count; i++) {
        setenv(php_esp32_env[2 * i], php_esp32_env[2 * i + 1], 1);
    }
    if (php_esp32_env_count > 0) {
        ESP_LOGI(TAG, "applied %d env var(s) from .env", php_esp32_env_count);
    }
}
```
<!-- @endcode-block -->

On boot with entries present, the firmware logs a line such as `applied 5 env var(s) from .env`; an empty table logs nothing and changes nothing.

## Build-time constant vs runtime state

A baked `.env` is one of three ways to get non-code data into a build, and it is the only one fixed at build time. Choose by whether the value is set by the *builder* or by the *running program*.

| Mechanism | Set by | Lifetime | Survives reboot? | Backing store | Read from PHP |
| --- | --- | --- | --- | --- | --- |
| `.env` (this page) | The build (`.env` file) | Constant for the whole run | N/A — recompiled each build | Internal flash, inside the app image | `$_ENV[...]`, `getenv(...)` |
| `store_*` (persistent) | The running PHP program | Across reboots | Yes | Dedicated NVS partition | `store_get()` / `store_set()` |
| `mem_*` (in-RAM) | The running PHP program | Until reset | No | RAM | `mem_get()` / `mem_set()` |

In short: bake it into the `.env` when the *builder* decides the value and it should be identical for every run of that image; use `store_*` when the *program* decides a value and it must outlive a reboot; use `mem_*` when the *program* needs shared state only for the current run. A baked variable cannot be written from PHP, and a `store_*`/`mem_*` value is not visible to `$_ENV`/`getenv()`.
