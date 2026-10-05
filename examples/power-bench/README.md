# power-bench

An automated power bench for a bare ESP32-S3 board (measured on an **ESP32-S3-Zero**). It is event-driven and
cycles three states, **5 s each**, printing a marker per state so each one's current can be read on a meter:

- **ACTIVE** — CPU awake (busy)
- **LIGHT SLEEP** — `esp_light_sleep`, timer wake (PSRAM retained)
- **DEEP SLEEP** — `esp_deep_sleep`, timer wake (full reboot → the cycle repeats)

Sleep comes from a small project extension (`firmware/exts/power`): `light_sleep(pin, ms)`, `deep_sleep(pin, ms)`
(both accept a timer timeout; `pin = -1` for timer-only wake), `wake_cause()`.

For the interactive device demo (screen on/off, buttons) on the S3-Touch-LCD, see `device-power-bench`.

## Measurements (AVHzY CT-3 inline on USB, whole board @5 V)

| State | Current | Power | vs previous |
|---|---|---|---|
| Active (CPU busy) | 0.04946 A | ~247 mW | — |
| Light sleep | 0.00188 A | ~9.4 mW | ~26× less than active |
| Deep sleep | 0.00056 A | ~2.8 mW | ~3.4× less than light sleep |

On this bare board deep sleep clearly beats light sleep (~3.4×), so it's worth using — unlike a
peripheral-heavy board (the S3-Touch-LCD shows deep ≈ light because a ~2 mA floor from the display, 8 MB PSRAM
and audio ICs masks it). A ~0.56 mA floor remains here (regulator quiescent + USB-serial-JTAG while USB is
connected); the chip's datasheet deep-sleep (~7 µA) needs battery power and a low-quiescent regulator, not USB.

Note: the serial log can't be captured headlessly during a run — the native USB suspends in light sleep and
disconnects in deep sleep. Read the current directly on the meter (the three states are distinct by magnitude).
