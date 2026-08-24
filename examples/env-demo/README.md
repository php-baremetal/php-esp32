# env-demo

Configuration from a `.env`, compiled into the firmware and read from PHP as `$_ENV` / `getenv()`.
Nothing is hardcoded in the script and nothing lives on a microSD card -- edit `.env`, rebuild, and
the values change.

## What it shows

The project ships a [`.env`](.env):

```sh
DEVICE_NAME=p4-lab-01
API_BASE=https://api.example.test/v1
SAMPLE_HZ=5
DEBUG=1
GREETING="hello from flash"
```

`phpflash build` bakes it into the firmware. `setup()` prints the loaded configuration and `loop()`
uses the `DEBUG` flag; the script reads everything through `$_ENV[...]` and `getenv(...)`:

```
=== env-demo :: configuration from .env ===
  device name : p4-lab-01
  API base    : https://api.example.test/v1
  sample rate : 5 Hz  (cast from the "5" string)
  debug       : on
  greeting    : hello from flash  (via getenv())
```

Env values are always strings, so `SAMPLE_HZ` is cast with `(int)` and `DEBUG` is compared as
`"1"`. Change any value in `.env`, run `phpflash build && phpflash flash`, and the new configuration
is compiled in.

## Configuring it

The feature is on by default whenever a `.env` exists. `php-esp32.config.toml` shows the knobs:

```toml
[env]
enabled = true      # false disables baking even if .env exists
file    = ".env"    # env file path, relative to the project
```

## Where the values live

They are compiled into the app image in internal flash -- not on the removable microSD, not in the
PHP source. That means they cannot be read by pulling the card, and are harder to extract than a file
on it. They are **not** secret or encrypted, though: a flash dump recovers them. See
[docs/environment.md](../../docs/environment.md) for the full story and the format rules.

No wiring, no card, no network: `storage_type` is `embedded`, so the script and its `.env` run
straight from flash on reset.
