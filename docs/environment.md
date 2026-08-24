# Build-time environment (`.env`)

A project can ship a `.env` file next to its `php-esp32.config.toml`. Its variables are baked into the
firmware at build time and exposed to PHP the usual way, as `$_ENV` and `getenv()`:

```php
$_ENV['APP_NAME'];      // "my-device"
getenv('API_BASE');     // "https://example.test"
```

This is for configuration a build should carry with it -- a device name, an endpoint, a feature flag,
a token -- without hardcoding it in the PHP or shipping it as a file on the card.

## Enabling it

The feature is on by default: if a `.env` sits beside the config, it is baked in. It is configurable
from `php-esp32.config.toml`:

```toml
[env]
enabled = true      # false turns it off even when .env exists
file    = ".env"    # env file path, relative to the project
```

With no `[env]` section, the default applies (baked when `.env` exists). `phpflash init` adds `.env`
to the project `.gitignore`.

## File format

One `KEY=VALUE` per line. Keys are C-style identifiers (`[A-Za-z_][A-Za-z0-9_]*`). Everything after
the first `=` is the value.

```sh
# a comment (only at the start of a line)
APP_NAME=my-device
API_BASE=https://example.test
GREETING="hello world"     # double or single quotes are stripped
export TOKEN=abc123        # a leading `export` is ignored
```

- Blank lines and lines starting with `#` are ignored.
- A matching pair of surrounding quotes is removed; inside double quotes, `\n`, `\t`, `\"` and `\\`
  are unescaped. Single quotes are literal.
- No shell expansion (`${VAR}` stays literal), no inline comments after a value, no multiline values.
- Values are strings -- `$_ENV['PORT']` is `"8080"`, not an int. Cast in PHP.

Change a value and rebuild; the new value is compiled in. Values are available for the whole run --
in an `init-loop` sketch and, in the `web-server` model, for every request.

## Where it lives, and what that means for secrets

The values are compiled into the application image in the chip's **internal flash** -- not written to
the microSD, and not part of your PHP source tree.

**They are not secret, and not encrypted.** Anyone who can read the flash -- physical access plus
`esptool`, or a debug port -- can recover them. Treat this as configuration, not as a vault.

What it *does* buy you, versus keeping the same values in a file on the card:

- **Not on removable media.** The card can be pulled, mounted on any PC and read in seconds; the
  baked-in values cannot. Swapping or cloning the microSD does not carry them.
- **Harder to extract.** Getting them back means dumping the flash over a wire, not reading a text
  file -- a meaningfully higher bar for a casual attacker, and they never travel with the card.

For values that must stay confidential against someone with the board in hand, use the SoC's own
protections -- flash encryption and secure boot -- which are outside the scope of this feature.

## How it works

`phpflash` reads the project's `.env`, generates a small C table (`build/php_env.gen.c`, never
committed) and passes it to the build. Before the engine starts, `main.c` applies each entry with
`setenv()`, so PHP's normal environment import puts them in `$_ENV` (the build's `variables_order`
carries `E`) and `getenv()` returns them. A build with no `.env` bakes an empty table -- nothing
changes.
