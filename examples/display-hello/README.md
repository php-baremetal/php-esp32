# display-hello

Minimal proof-of-concept for the SPI/QSPI display path: bring up an ST77916 panel and cycle the
background colour. It only proves the panel lights up and paints — text/graphics come later.

`Baremetal\Spi\Bus` owns the SPI host and its shared pins (QSPI here: four data lines). The ST77916
panel is a device on that bus implementing the bus-agnostic `Output\Display` capability.

```php
use Baremetal\Spi\Bus;
use Baremetal\Spi\Driver\St77916;

$bus = new Bus(sclk: 40, mosi: 46, miso: 45, data2: 42, data3: 41);   // QSPI: mosi=D0, miso=D1
$lcd = new St77916($bus, cs: 21, rst: 3, bl: 5, width: 360, height: 360);

$lcd->fill($lcd->rgb(0, 0, 255));   // whole screen blue
```

## The panel is configurable

Size and mounting are constructor parameters, so one driver covers different ST77916 panels:

| Param | Default | Meaning |
|---|---|---|
| `cs`, `rst`, `bl` | 21, 3, -1 | chip-select, reset, backlight pins (`bl = -1` leaves it alone) |
| `width`, `height` | 360, 360 | panel resolution (e.g. 240×240, 172×320, …) |
| `x_gap`, `y_gap` | 0, 0 | GRAM offset — round/small panels often need one |
| `mirror_x`, `mirror_y`, `swap_xy` | false | orientation / rotation |
| `invert` | false | colour inversion |

## API (proof-of-concept)

```
fill(int $color)          // paint the whole panel one RGB565 colour
rgb(int $r, int $g, int $b): int   // -> RGB565
```

Colours are RGB565 ints; `rgb()` builds one. `instanceof Baremetal\Output\Display` is the presence
check, whatever the wire.

## Run

```sh
phpflash flash
phpflash monitor      # the panel cycles red → green → blue → white
```
