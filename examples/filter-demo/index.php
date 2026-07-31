<?php
// ext/filter: validate and sanitize values with filter_var(). Needs a firmware built
// with the filter extension: run ./flash.sh and answer "y" to filter, or build with
// idf.py -DPHP_EXT_FILTER=ON.

echo "email validation:\n";
foreach (['dev@example.com', 'not-an-email', 'a@b.c'] as $e) {
    $r = filter_var($e, FILTER_VALIDATE_EMAIL);
    printf("  %-18s %s\n", $e, $r === false ? 'INVALID' : "ok ($r)");
}

echo "\ntyped validation:\n";
var_dump(filter_var('42', FILTER_VALIDATE_INT));
var_dump(filter_var('3.14', FILTER_VALIDATE_FLOAT));
var_dump(filter_var('on', FILTER_VALIDATE_BOOLEAN));
var_dump(filter_var('https://php.net/', FILTER_VALIDATE_URL) !== false);
var_dump(filter_var('192.168.1.1', FILTER_VALIDATE_IP) !== false);
var_dump(filter_var('999.1.1.1', FILTER_VALIDATE_IP));   // bad IP -> false

echo "\nsanitize:\n";
echo '  full_special: ' . filter_var('<b>hi</b> & "you"', FILTER_SANITIZE_FULL_SPECIAL_CHARS) . "\n";
echo '  number_int:   ' . filter_var('a1b2c3!@#', FILTER_SANITIZE_NUMBER_INT) . "\n";

echo "\nvalidate with a range option:\n";
foreach (['50', '150'] as $n) {
    $v = filter_var($n, FILTER_VALIDATE_INT, ['options' => ['min_range' => 0, 'max_range' => 100]]);
    echo "  $n in [0,100]? " . ($v === false ? 'no' : 'yes') . "\n";
}
