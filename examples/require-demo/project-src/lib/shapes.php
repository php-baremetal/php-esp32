<?php
// Shape classes kept in their own file and pulled in with require_once, so the
// definitions live apart from the code that uses them. require_once makes sure
// the classes are only declared once even if the file is required again.

interface Shape {
    public function name(): string;
    public function area(): float;
}

final class Circle implements Shape {
    public function __construct(private float $r) {}
    public function name(): string { return 'circle'; }
    public function area(): float { return M_PI * $this->r ** 2; }
}

final class Rectangle implements Shape {
    public function __construct(private float $w, private float $h) {}
    public function name(): string { return 'rectangle'; }
    public function area(): float { return $this->w * $this->h; }
}
