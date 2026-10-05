# device-power-bench

An event-driven power demo and measurement bench on the ESP32-S3-Touch-LCD-1.85B. (For a bare automated
active/light/deep cycle on an S3-Zero, see `power-bench`.)

The screen is red while active. A **click** on BOOT turns the screen off and drops into **light sleep**; any
BOOT press wakes it. A **2 s hold** on BOOT enters **deep sleep** (a BOOT press reboots). The hardware **PWR**
button is a power latch (hold ~2 s = fully off, press = on) — not readable by firmware.

Buttons come from the core **`watch_button`** source: it owns debounce + press/hold/release timing in C and
emits `Click`/`Held` (also `Pressed`/`Released`/`Repeat`/`DoubleClick`), so the script never polls. Sleep uses
a small project extension (`firmware/exts/power`): `light_sleep()`, `deep_sleep()`, `wake_cause()`. The sampler
is paused (`button_sampling(false)`) before a light sleep so the chip can actually sleep; the panel is
re-initialised on wake (`$lcd->wake()`) because light sleep disturbs the QSPI lines.

## Measurements (AVHzY CT-3 inline on USB, whole board @5 V)

| State | Current | Power | vs active |
|---|---|---|---|
| Active (screen red) | 0.0740 A | ~370 mW | — |
| Light sleep (screen off) | 0.0030 A | ~15 mW | ~25× less |
| Deep sleep | 0.0020 A | ~10 mW | ~37× less |

**Conclusion:** light sleep (3 mA, RAM retained, instant wake) is the practical low-power regime. Deep sleep
beats it by only ~1 mA here — not worth the full reboot and lost RAM — because the board has a ~2 mA floor
(regulators + USB-serial-JTAG) when measured over USB. A battery-optimised board would show a far lower chip
deep-sleep figure; this bench measures the whole board at 5 V.

Note: on the S3, entering deep sleep with USB connected can trigger a `USB_UART_CHIP_RESET`; a clean deep-sleep
figure is best taken on battery.
