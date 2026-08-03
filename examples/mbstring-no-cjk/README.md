# mbstring-no-cjk

The same `mb_*` functions as [`mbstring-demo`](../mbstring-demo/), but on a firmware built
with the **CJK encodings dropped**. This is the sub-option that makes mbstring small: the
legacy Chinese/Japanese/Korean codecs (Shift-JIS, EUC-JP/CN/KR, Big5, GB18030, ISO-2022-*,
and the Japanese `mb_convert_kana()`) are the bulk of the extension. Leaving them out takes
mbstring from **~965 KB down to ~209 KB** — a ~755 KB saving — while UTF-8, UTF-16, ASCII
and the Latin/single-byte charsets are untouched.

## Firmware

This project's `php-esp32.config.toml` enables mbstring with the CJK sub-option
(`[extensions.mbstring]` with `no_cjk = true`), so `phpflash build` builds the slim mbstring
with no flags to pass. With a full mbstring firmware (`no_cjk = false`) this same script still
runs — the CJK rows just show `present` instead of `dropped`, and the `SJIS` conversion
succeeds.

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

To run from a microSD instead, copy `project-src/index.php` to the card root and reset.

## Output (excerpt)

```
mb_strlen:       19 chars, 27 bytes
mb_strtoupper:   CITTÀ — €10 — NAÏVE
mb_substr(0,6):  Città
utf8->latin1->utf8: Città

encodings available: 44
  UTF-8       present
  UTF-16      present
  ISO-8859-1  present
  ASCII       present
  SJIS        dropped
  EUC-JP      dropped
  BIG-5       dropped
  GB18030     dropped

convert 'A' to SJIS: unavailable — mb_convert_encoding(): Argument #2 ($to_encoding) must be a valid encoding, "SJIS" given
```

On the full-mbstring firmware the same script reports **79** encodings and the CJK rows show
`present` (and the `SJIS` conversion succeeds) — the trim drops 35 encodings.
