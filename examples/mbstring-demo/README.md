# mbstring-demo

Multibyte-safe string functions: `mb_strlen`, `mb_substr`, `mb_strtoupper` /
`mb_strtolower`, `mb_strpos`, `mb_detect_encoding`, `mb_convert_encoding`,
`mb_str_split`, `mb_convert_case`. The example mixes accents, an em dash and some
Japanese to show byte-length vs character-length.

## Firmware

`ext/mbstring` is an optional extension, off by default. Build a firmware with it on:

- `./flash.sh` → answer **y** to `Include mbstring`, or
- `idf.py -DPHP_EXT_MBSTRING=ON ...`

It is built **without** the `mb_ereg*` regex family (that would need the oniguruma
library, which isn't ported); everything else works. This is the heavy extension —
the bundled libmbfl carries the CJK conversion tables — see
[`docs/footprint.md`](../../docs/footprint.md).

## Run

Copy `index.php` to the microSD as `/index.php`, reset the board, watch the serial port.

## Output (excerpt)

```
internal encoding: UTF-8
string:            Città di Località — こんにちは
strlen (bytes):    39
mb_strlen (chars): 25
mb_strtoupper:     CITTÀ
mb_strtolower:     città
mb_substr(0,8):    Città di
mb_strpos('こ'):    20

detect + convert:
  detect: UTF-8
  utf8 'Città' -> latin1 = 5 bytes (round-trip: Città)

mb_str_split('café'): c|a|f|é
mb_convert_case(TITLE): Éléphant Bleu
```
