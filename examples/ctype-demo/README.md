# ctype-demo

The `ctype_*` character-class checks: `ctype_alpha`, `ctype_digit`, `ctype_alnum`,
`ctype_space`, `ctype_xdigit`. Fast whole-string tests with no allocation.

## Firmware

`ext/ctype` is an optional extension, off by default. Build a firmware with it on:

- `./flash.sh` → answer **y** to `Include ctype`, or
- `idf.py -DPHP_EXT_CTYPE=ON ...`

It's tiny: one source file, no data tables.

## Run

Copy `index.php` to the microSD as `/index.php`, reset the board, watch the serial port.

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
