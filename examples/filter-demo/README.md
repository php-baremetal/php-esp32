# filter-demo

Validation and sanitization with `filter_var()`: emails, ints, floats, booleans,
URLs, IPs, plus a couple of sanitizers and a range-constrained integer.

## Firmware

`ext/filter` is an optional extension, off by default. This project's `php-esp32.config.toml`
already enables it (`[extensions.filter]`), so you don't pass any build flags. It builds on
`ext/pcre` (always in) and `ext/standard`.

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

To run from a microSD instead, copy `project-src/index.php` to the card root and reset.

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
