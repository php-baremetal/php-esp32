<?php
// Medium reference file: a small validator/collection class with a handful of
// methods. Definitions only (no execution) -- this is what gets compiled.

final class BenchMedium
{
    /** @var array<int, array<string, mixed>> */
    private array $rows = [];
    private array $errors = [];

    public function __construct(private string $name = 'set') {}

    public function add(array $row): static
    {
        $this->rows[] = $row;
        return $this;
    }

    public function count(): int
    {
        return count($this->rows);
    }

    public function column(string $key): array
    {
        return array_map(static fn($r) => $r[$key] ?? null, $this->rows);
    }

    public function where(string $key, mixed $value): array
    {
        return array_values(array_filter(
            $this->rows,
            static fn($r) => ($r[$key] ?? null) === $value,
        ));
    }

    public function sortBy(string $key): static
    {
        usort($this->rows, static fn($a, $b) => ($a[$key] ?? 0) <=> ($b[$key] ?? 0));
        return $this;
    }

    public function sum(string $key): int|float
    {
        $total = 0;
        foreach ($this->rows as $row) {
            $total += $row[$key] ?? 0;
        }
        return $total;
    }

    public function validate(array $rules): bool
    {
        $this->errors = [];
        foreach ($this->rows as $i => $row) {
            foreach ($rules as $key => $rule) {
                if ($rule === 'required' && !isset($row[$key])) {
                    $this->errors[] = "row $i: missing '$key'";
                } elseif ($rule === 'int' && isset($row[$key]) && !is_int($row[$key])) {
                    $this->errors[] = "row $i: '$key' not an int";
                }
            }
        }
        return $this->errors === [];
    }

    public function errors(): array
    {
        return $this->errors;
    }

    public function toJson(): string
    {
        return json_encode(['name' => $this->name, 'rows' => $this->rows]) ?: '{}';
    }
}
