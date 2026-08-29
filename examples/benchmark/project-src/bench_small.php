<?php
// Small reference file: one tiny class. Definitions only (no execution).

final class BenchSmall
{
    public function __construct(private int $seed = 1) {}

    public function next(): int
    {
        return $this->seed = ($this->seed * 1103515245 + 12345) & 0x7fffffff;
    }

    public function between(int $lo, int $hi): int
    {
        return $lo + $this->next() % (($hi - $lo) + 1);
    }
}
