<?php
// Large reference file: a fuller in-memory table with a chainable query API.
// Definitions only (no execution) -- included to measure the compiled size of a
// larger, framework-flavoured class.

final class BenchLarge
{
    /** @var array<int, array<string, mixed>> */
    private array $rows = [];
    /** @var array<string, callable> */
    private array $filters = [];
    private array $order = [];
    private ?int $limit = null;
    private int $offset = 0;

    public function __construct(private string $table = 'items') {}

    public function insert(array $row): static
    {
        $this->rows[] = $row;
        return $this;
    }

    public function insertMany(array $rows): static
    {
        foreach ($rows as $row) {
            $this->rows[] = $row;
        }
        return $this;
    }

    public function truncate(): static
    {
        $this->rows = [];
        return $this;
    }

    public function whereEquals(string $key, mixed $value): static
    {
        $this->filters[] = static fn($r) => ($r[$key] ?? null) === $value;
        return $this;
    }

    public function whereGreater(string $key, int|float $value): static
    {
        $this->filters[] = static fn($r) => ($r[$key] ?? 0) > $value;
        return $this;
    }

    public function whereLess(string $key, int|float $value): static
    {
        $this->filters[] = static fn($r) => ($r[$key] ?? 0) < $value;
        return $this;
    }

    public function whereIn(string $key, array $values): static
    {
        $this->filters[] = static fn($r) => in_array($r[$key] ?? null, $values, true);
        return $this;
    }

    public function whereLike(string $key, string $needle): static
    {
        $this->filters[] = static fn($r) => str_contains((string) ($r[$key] ?? ''), $needle);
        return $this;
    }

    public function orderBy(string $key, string $dir = 'asc'): static
    {
        $this->order[] = [$key, strtolower($dir) === 'desc' ? -1 : 1];
        return $this;
    }

    public function limit(int $limit, int $offset = 0): static
    {
        $this->limit = $limit;
        $this->offset = $offset;
        return $this;
    }

    private function apply(): array
    {
        $rows = $this->rows;
        foreach ($this->filters as $filter) {
            $rows = array_filter($rows, $filter);
        }
        $rows = array_values($rows);
        foreach (array_reverse($this->order) as [$key, $sign]) {
            usort($rows, static fn($a, $b) => $sign * (($a[$key] ?? 0) <=> ($b[$key] ?? 0)));
        }
        if ($this->limit !== null) {
            $rows = array_slice($rows, $this->offset, $this->limit);
        }
        return $rows;
    }

    public function get(): array
    {
        $rows = $this->apply();
        $this->reset();
        return $rows;
    }

    public function first(): ?array
    {
        return $this->limit(1)->get()[0] ?? null;
    }

    public function count(): int
    {
        return count($this->apply());
    }

    public function pluck(string $key): array
    {
        return array_map(static fn($r) => $r[$key] ?? null, $this->apply());
    }

    public function sum(string $key): int|float
    {
        $total = 0;
        foreach ($this->apply() as $row) {
            $total += $row[$key] ?? 0;
        }
        return $total;
    }

    public function avg(string $key): float
    {
        $rows = $this->apply();
        return $rows === [] ? 0.0 : $this->sum($key) / count($rows);
    }

    public function min(string $key): mixed
    {
        $values = $this->pluck($key);
        return $values === [] ? null : min($values);
    }

    public function max(string $key): mixed
    {
        $values = $this->pluck($key);
        return $values === [] ? null : max($values);
    }

    public function groupBy(string $key): array
    {
        $groups = [];
        foreach ($this->apply() as $row) {
            $groups[$row[$key] ?? '_'][] = $row;
        }
        return $groups;
    }

    public function toJson(): string
    {
        return json_encode(['table' => $this->table, 'rows' => $this->apply()]) ?: '{}';
    }

    private function reset(): void
    {
        $this->filters = [];
        $this->order = [];
        $this->limit = null;
        $this->offset = 0;
    }
}
