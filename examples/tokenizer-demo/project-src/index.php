<?php
// ext/tokenizer: break PHP source into tokens using the engine's own lexer. This is what
// frameworks and static-analysis tools lean on. Runs entirely in memory -- no filesystem.

$code = '<?php $sum = 1 + 2; echo "sum=", $sum;';

echo "tokenizing: $code\n\n";
foreach (token_get_all($code) as $t) {
    if (is_array($t)) {
        printf("  %-16s %s\n", token_name($t[0]), trim($t[1]));
    } elseif (trim($t) !== '') {
        printf("  %-16s %s\n", "(punctuation)", $t);
    }
}

// The PhpToken class (PHP 8) is the object-oriented API over the same lexer.
$tokens = PhpToken::tokenize('<?php echo strtoupper("ok");');
$names  = array_map(fn($t) => $t->getTokenName(), $tokens);
echo "\nPhpToken::tokenize -> ", count($tokens), " tokens: ", implode(' ', $names), "\n";
