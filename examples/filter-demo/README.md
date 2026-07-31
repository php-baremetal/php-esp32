# filter-demo

Validation and sanitization with `filter_var()`: emails, ints, floats, booleans,
URLs, IPs, plus a couple of sanitizers and a range-constrained integer.

## Firmware

`ext/filter` is an optional extension, off by default. Build a firmware with it on:

- `./flash.sh` → answer **y** to `Include filter`, or
- `idf.py -DPHP_EXT_FILTER=ON ...`

It builds on `ext/pcre` (always in) and `ext/standard`.

## Run

Copy `index.php` to the microSD as `/index.php`, reset the board, watch the serial port.

## Output (excerpt)

```
email validation:
  dev@example.com    ok (dev@example.com)
  not-an-email       INVALID
  a@b.c              ok (a@b.c)

typed validation:
int(42)
float(3.14)
bool(true)
...

validate with a range option:
  50 in [0,100]? yes
  150 in [0,100]? no
```
