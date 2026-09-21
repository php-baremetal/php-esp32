# i2c-scan

Scan an I2C bus from PHP and print every device that answers.

```php
use Baremetal\I2c\Bus;

$bus = new Bus(sda: 11, scl: 10);
foreach ($bus->scan() as $addr => $info) {
    printf("  0x%02X  %s\n", $addr, $info['driver'] ?? '(no driver)');
}
```

`Bus` is a handle onto an I2C master bus, keyed by its pins: constructing it twice with the same
pins returns the same underlying bus. `scan()` sweeps 0x08..0x77 under the bus lock and returns the
addresses that ACKed; `$info['driver']` names the driver mounted at an address, or is `null` when
none is (raw addresses).

For a raw device without a driver:

```php
$dev = $bus->device(0x68);          // or new Baremetal\I2c\Device($bus, 0x68)
$dev->probe();                      // bool: does it ACK?
$who = $dev->readReg(0x75);         // read 1 byte from a register
$dev->writeReg(0x6B, 0x00);         // write a register
```

## Run

```sh
phpflash flash          # from this directory
phpflash monitor
```

Enable the extension in your own project with:

```toml
[extensions.i2c]
enabled = true
```

## Wiring

Share `SDA`, `SCL`, `3V3` and `GND` across the devices. Set `SDA`/`SCL` in `index.php` to the pins
your board exposes for I2C (the example defaults to `SDA=11`, `SCL=10`).
