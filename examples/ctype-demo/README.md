# ctype-demo

The `ctype_*` character-class checks: `ctype_alpha`, `ctype_digit`, `ctype_alnum`,
`ctype_space`, `ctype_xdigit`. Fast whole-string tests with no allocation.

## Firmware

`ext/ctype` is an optional extension, off by default. This project's `php-esp32.config.toml`
already enables it (`[extensions.ctype]`), so you don't pass any build flags. It's tiny: one
source file, no data tables.

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

To run from a microSD instead, copy `project-src/index.php` to the card root and reset.

## Output (excerpt)

```
input      alpha  digit  alnum  space  xdigit
"HELLO"    yes    -      yes    -      -
"Hello123" -      -      yes    -      -
"12345"    -      yes    yes    -      yes
"ff00aa"   -      -      yes    -      yes
"   "      -      -      -      yes    -

identifier check:
  user_id  -> ok
  2fast    -> no
```
