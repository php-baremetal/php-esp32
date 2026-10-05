# power-save

Verify `power_save` (automatic light sleep) on an ESP32-S3-Zero.

**The automatic light sleep is already in the firmware: in the event-driven model
the reactor sleeps on its own between one event and the next — free, without you
calling anything.** With `power_save = true` in `php-esp32.config.toml`, the build
enables `esp_pm` tickless idle: whenever the reactor is idle (blocked on the event
queue) the chip drops into light sleep by itself, RAM retained, instant wake on
the next event. No code change needed.

See the [Power save recipe](../../docs/recipes/power-save.md) for the full write-up.

The script schedules one heartbeat every 10 s and does nothing else, so the
board sits in light sleep almost the whole time. Every third beat it brackets a
~2 s busy stretch with `power_hold()` / `power_release()` to show the current
rising while sleep is forbidden, then dropping back.

```php
power_hold();        // forbid light sleep
// ... time-critical work ...
power_release();     // allow it again
```

The reactor also auto-releases any hold left standing at the end of each event
handler, so a forgotten `power_release()` can't pin the chip awake forever.

`I2cBus::CORE1` is rejected under `power_save`: the core-1 polling loop keeps the
CPU busy and would defeat light sleep — use `I2cBus::SYNC` instead.

## Measuring

Put a current meter (e.g. AVHzY CT-3) inline on the supply. Expect a low idle
current between beats, with a step up during the ~2 s `power_hold` window.

Caveat: with the USB data cable attached, the USB-Serial/JTAG driver can hold a
power-management lock that keeps the chip from actually entering light sleep, so
the idle current may not drop. For a clean reading, power the board from 5 V
externally with the USB data cable disconnected.

## Flashing the S3-Zero

`phpflash flash` can fail on the S3-Zero, and once this firmware runs it sleeps
and drops the USB-Serial/JTAG, so esptool's reset-into-download also fails. Put
the board in download mode by hand (hold BOOT, tap RESET), then:

```sh
esptool --chip esp32s3 -p /dev/ttyACM0 --before no_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size 4MB --flash_freq 80m \
  0x0 build/compiled/bootloader/bootloader.bin \
  0x8000 build/compiled/partition_table/partition-table.bin \
  0x10000 build/compiled/php-esp32.bin \
  0x370000 build/compiled/storage.bin
```

After flashing it may stay in download mode — tap RESET alone to boot the app.
