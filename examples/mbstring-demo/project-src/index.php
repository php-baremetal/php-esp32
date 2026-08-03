<?php
// ext/mbstring: multibyte-safe string functions (mb_*). Needs a firmware built with
// the mbstring extension: run ./flash.sh and answer "y" to mbstring, or build with
// idf.py -DPHP_EXT_MBSTRING=ON. It is built WITHOUT the mb_ereg* regex family (no
// oniguruma) -- everything else (length, case, substr, convert, detect) is here.

$s = 'Città di Località — こんにちは';   // accents, an em dash, some Japanese

echo "internal encoding: " . mb_internal_encoding() . "\n";
echo "string:            $s\n";
echo "strlen (bytes):    " . strlen($s) . "\n";
echo "mb_strlen (chars): " . mb_strlen($s) . "\n";
echo "mb_strtoupper:     " . mb_strtoupper('città') . "\n";
echo "mb_strtolower:     " . mb_strtolower('CITTÀ') . "\n";
echo "mb_substr(0,8):    " . mb_substr($s, 0, 8) . "\n";
echo "mb_strpos('こ'):    " . mb_strpos($s, 'こ') . "\n";

echo "\ndetect + convert:\n";
echo "  detect: " . mb_detect_encoding($s, ['ASCII', 'UTF-8']) . "\n";
$latin = mb_convert_encoding('Città', 'ISO-8859-1', 'UTF-8');
echo "  utf8 'Città' -> latin1 = " . strlen($latin) . " bytes"
   . " (round-trip: " . mb_convert_encoding($latin, 'UTF-8', 'ISO-8859-1') . ")\n";

echo "\nmb_str_split('café'): " . implode('|', mb_str_split('café')) . "\n";

// exactly the kind of thing Str::title() does under the hood
echo "mb_convert_case(TITLE): " . mb_convert_case('éLÉphant bleu', MB_CASE_TITLE) . "\n";
