# tokenizer-demo

`ext/tokenizer` on the microcontroller: break PHP source into tokens with the engine's own lexer —
`token_get_all()`, `token_name()`, and the `PhpToken` class. This is what frameworks and
static-analysis tools rely on. It runs entirely in memory, so it needs no card and no network, and
works on any board.

## What it does

```php
foreach (token_get_all('<?php $sum = 1 + 2;') as $t) {
    echo is_array($t) ? token_name($t[0]) : $t, "\n";   // T_OPEN_TAG, T_VARIABLE, T_LNUMBER, ...
}

$tokens = PhpToken::tokenize('<?php echo strtoupper("ok");');   // the OO API over the same lexer
```

Enable it with `[extensions.tokenizer] enabled = true` (~13 KB).

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

## The output

From [`monitor.txt`](monitor.txt) (a real run):

```
tokenizing: <?php $sum = 1 + 2; echo "sum=", $sum;

  T_OPEN_TAG       <?php
  T_VARIABLE       $sum
  T_LNUMBER        1
  (punctuation)    +
  T_LNUMBER        2
  T_ECHO           echo
  T_CONSTANT_ENCAPSED_STRING "sum="
  ...

PhpToken::tokenize -> 8 tokens: T_OPEN_TAG T_ECHO T_WHITESPACE T_STRING ( T_CONSTANT_ENCAPSED_STRING ) ;
```
