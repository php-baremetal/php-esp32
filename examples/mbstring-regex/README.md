# mbstring-regex

The multibyte-regex family — `mb_split`, `mb_ereg`, `mb_ereg_replace`, `mb_ereg_match` —
running on the real **Oniguruma** engine, so Unicode-aware patterns (`\p{L}` and other
character properties) work on UTF-8 text.

## Firmware

`mb_ereg*` / `mb_split` need a regex engine, which PHP doesn't bundle. This project's
`php-esp32.config.toml` enables mbstring with Oniguruma (`[extensions.mbstring]` with
`onig = true`), so `phpflash build` fetches the library and builds it in — no flags to pass. It
adds ~445 KB on top of mbstring — see [`docs/reference/footprint.md`](../../docs/reference/footprint.md). If you only
need `mb_split` with simple patterns, a PCRE polyfill (see `eloquent-demo`) avoids this cost; the
difference is the Unicode-property support shown below.

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

To run from a microSD instead, copy `project-src/index.php` to the card root and reset.

## Output

```
input: Città 42 — naïve 7 — café 100

mb_split on ' — ':
  [Città 42]
  [naïve 7]
  [café 100]

mb_ereg_replace (mask the numbers):
  Città # — naïve # — café #

mb_ereg with a Unicode property (\p{L}+ followed by digits):
  first match: word="Città" number=42

mb_ereg_match (does it start with a letter?): bool(true)
```
