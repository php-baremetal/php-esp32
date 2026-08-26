# mbstring-demo

Multibyte-safe string functions: `mb_strlen`, `mb_substr`, `mb_strtoupper` /
`mb_strtolower`, `mb_strpos`, `mb_detect_encoding`, `mb_convert_encoding`,
`mb_str_split`, `mb_convert_case`. The example mixes accents, an em dash and some
Japanese to show byte-length vs character-length.

## Firmware

`ext/mbstring` is an optional extension, off by default. This project's `php-esp32.config.toml`
already enables it (`[extensions.mbstring]`), so you don't pass any build flags. It is built
**without** the `mb_ereg*` regex family (that needs oniguruma — see
[`mbstring-regex`](../mbstring-regex/)); everything else works. This is the heavy extension —
the bundled libmbfl carries the CJK conversion tables — see
[`docs/reference/footprint.md`](../../docs/reference/footprint.md).

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

To run from a microSD instead, copy `project-src/index.php` to the card root and reset.

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
