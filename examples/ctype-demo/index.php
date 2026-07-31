<?php
// ext/ctype: fast character-class checks on whole strings (ctype_alpha, ctype_digit,
// ...). Needs a firmware built with the ctype extension: run ./flash.sh and answer
// "y" to ctype, or build with idf.py -DPHP_EXT_CTYPE=ON.
//
// ctype is tiny: one source file, no data tables.

$samples = ['HELLO', 'Hello123', 'abc', '12345', 'ff00aa', '   ', '+39 06'];

printf("%-10s %-6s %-6s %-6s %-6s %-7s\n", 'input', 'alpha', 'digit', 'alnum', 'space', 'xdigit');
foreach ($samples as $s) {
    printf("%-10s %-6s %-6s %-6s %-6s %-7s\n",
        '"' . $s . '"',
        ctype_alpha($s)  ? 'yes' : '-',
        ctype_digit($s)  ? 'yes' : '-',
        ctype_alnum($s)  ? 'yes' : '-',
        ctype_space($s)  ? 'yes' : '-',
        ctype_xdigit($s) ? 'yes' : '-');
}

// A tiny practical use: does this look like a PHP identifier?
echo "\nidentifier check:\n";
foreach (['user_id', '2fast', '_tmp', 'a-b'] as $id) {
    $ok = $id !== ''
        && (ctype_alpha($id[0]) || $id[0] === '_')
        && ctype_alnum(str_replace('_', '', $id));
    echo "  " . str_pad($id, 8) . " -> " . ($ok ? 'ok' : 'no') . "\n";
}
