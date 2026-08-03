<?php
// mb_ereg* / mb_split -- the multibyte regex family. Available only when mbstring is
// built WITH oniguruma: run ./flash.sh, answer "y" to mbstring, then "y" to the
// "mb_ereg*/mb_split regex" question -- or build with
// idf.py -DPHP_EXT_MBSTRING=ON -DPHP_EXT_MBSTRING_ONIG=ON.
//
// Unlike a PCRE-based mb_split polyfill, this is the real Oniguruma engine, so
// Unicode-aware patterns (\p{L}, character properties) work on multibyte text.

mb_regex_encoding('UTF-8');

$s = 'Città 42 — naïve 7 — café 100';
echo "input: $s\n\n";

echo "mb_split on ' — ':\n";
foreach (mb_split(' — ', $s) as $part) {
    echo "  [$part]\n";
}

echo "\nmb_ereg_replace (mask the numbers):\n";
echo '  ' . mb_ereg_replace('[0-9]+', '#', $s) . "\n";

echo "\nmb_ereg with a Unicode property (\\p{L}+ followed by digits):\n";
if (mb_ereg('([\p{L}]+)\s+([0-9]+)', $s, $m)) {
    printf("  first match: word=\"%s\" number=%s\n", $m[1], $m[2]);
}

echo "\nmb_ereg_match (does it start with a letter?): ";
var_dump(mb_ereg_match('\p{L}', $s));
