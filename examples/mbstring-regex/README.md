# mbstring-regex

The multibyte-regex family — `mb_split`, `mb_ereg`, `mb_ereg_replace`, `mb_ereg_match` —
running on the real **Oniguruma** engine, so Unicode-aware patterns (`\p{L}` and other
character properties) work on UTF-8 text.

## Firmware

`mb_ereg*` / `mb_split` are **off by default**: PHP doesn't bundle a regex engine, so this
example needs a firmware where mbstring is built with Oniguruma:

- `./flash.sh` → answer **y** to `mbstring`, then **y** to the *"mb_ereg\*/mb_split regex"*
  question (it fetches the library on demand), or
- `idf.py -DPHP_EXT_MBSTRING=ON -DPHP_EXT_MBSTRING_ONIG=ON ...` (run `./scripts/fetch-oniguruma.sh`
  first).

It adds ~445 KB on top of mbstring — see [`docs/footprint.md`](../../docs/footprint.md). If you
only need `mb_split` with simple patterns, a PCRE polyfill (see `eloquent-demo`) avoids this
cost; the difference is the Unicode-property support shown below.

## Run

Copy `index.php` to the microSD as `/index.php`, reset the board, watch the serial port.

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
