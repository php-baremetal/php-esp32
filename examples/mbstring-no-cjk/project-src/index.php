<?php
// ext/mbstring built with -DPHP_EXT_MBSTRING_NO_CJK=ON: the multibyte functions are all
// here, but the legacy CJK encodings (Shift-JIS, EUC-JP/CN/KR, Big5, GB18030, ...) are
// dropped to save ~740 KB. UTF-8, UTF-16, ASCII and the Latin/single-byte charsets are
// unaffected. Build with ./flash.sh (answer "y" to mbstring, then "y" to "drop CJK").

$s = 'Città — €10 — naïve';
echo "mb_strlen:       " . mb_strlen($s) . " chars, " . strlen($s) . " bytes\n";
echo "mb_strtoupper:   " . mb_strtoupper($s) . "\n";
echo "mb_substr(0,6):  " . mb_substr($s, 0, 6) . "\n";

// UTF-8 <-> Latin-1 still works
$latin = mb_convert_encoding('Città', 'ISO-8859-1', 'UTF-8');
echo "utf8->latin1->utf8: " . mb_convert_encoding($latin, 'UTF-8', 'ISO-8859-1') . "\n";

// What survived the trim: UTF/Latin present, the CJK codecs gone.
$all  = array_map('strtoupper', mb_list_encodings());
$have = fn(string $e): bool => in_array(strtoupper($e), $all, true);

echo "\nencodings available: " . count($all) . "\n";
foreach (['UTF-8', 'UTF-16', 'ISO-8859-1', 'ASCII', 'SJIS', 'EUC-JP', 'BIG-5', 'GB18030'] as $e) {
    echo "  " . str_pad($e, 12) . ($have($e) ? 'present' : 'dropped') . "\n";
}

// Converting to a dropped encoding fails cleanly (a ValueError, not a crash)
echo "\nconvert 'A' to SJIS: ";
try {
    $r = mb_convert_encoding('A', 'SJIS', 'UTF-8');
    echo "0x" . bin2hex($r) . " (still present?!)\n";
} catch (\ValueError $e) {
    echo "unavailable — " . $e->getMessage() . "\n";
}
