---
eyebrow: 'Docs · Recipes'
lede:    'Cut idle power with one build flag: power_save turns on automatic light sleep, so an event-driven device sleeps between events for free — plus power_hold() for the rare stretch that must stay awake.'
see_also:
  - href: ./event-driven.md
    meta: 'Recipes'
    label: 'Event-driven basics'
  - href: ./read-a-button.md
    meta: 'Recipes'
    label: 'Read a button'
prev:
  label: 'Event-driven basics'
  href: ./event-driven.md
next:
  label: 'Read a button'
  href: ./read-a-button.md
---

# Power save

## Automatic light sleep — free, nothing to call

The light sleep is **already in the firmware**: in the event-driven model the reactor sleeps on its own between one event and the next, free, without you calling anything. Turn it on with one line in `php-esp32.config.toml`:

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
power_save = true
```
<!-- @endcode-block -->

This enables `esp_pm` tickless idle. Whenever the reactor is idle — blocked waiting for the next event — the chip drops into light sleep by itself: RAM is retained and it wakes instantly on the next timer, GPIO or request. No sleep code in your PHP.

On a bare ESP32-S3-Zero, a script with one heartbeat every 10 s idled at **~0.95 mA** (from ~49 mA active) — about a 50x drop — measured on external 5 V.

This pairs naturally with the [event-driven model](./event-driven.md): the more your device waits on events instead of looping, the more of the time it spends asleep.

## Staying awake when it matters

For a short time-critical stretch, forbid light sleep around it:

<!-- @code-block language="php" label="power_hold / power_release" -->
```php
power_hold();        // no light sleep from here
// ... time-critical work ...
power_release();     // allow it again
```
<!-- @endcode-block -->

`power_hold()` can nest; each call needs a matching `power_release()`. As a safety net, the reactor auto-releases any hold still standing at the end of each event handler, so a forgotten `power_release()` can't pin the chip awake forever.

## What doesn't mix with power_save

An `I2cBus::CORE1` bus is **rejected** under `power_save` (the constructor throws): its core-1 polling loop keeps the CPU busy and would defeat light sleep. Use the default `I2cBus::SYNC` instead.

## Measuring it

Put a current meter inline on the supply and expect a low idle current, with a step up during any `power_hold()` window.

<!-- @callout variant="note" title="Measure from external 5 V, not over USB" -->
With the USB data cable attached, the USB-Serial/JTAG driver can hold a power-management lock that keeps the chip from actually entering light sleep, so the idle current may not drop. For a clean reading, power the board from 5 V externally with the USB data cable disconnected.
<!-- @endcallout -->

## Explicit sleep (deep sleep)

Explicit `light_sleep()` / `deep_sleep()` calls are not built into the firmware. With `power_save` you rarely need an explicit light sleep. Deep sleep tears everything down and depends on the board's wake topology (which pin wakes it, pull-ups, state saved to NVS to resume), so for now it lives as a small per-project C extension you can copy and adapt — see the [`power-bench`](../../examples/power-bench/) and [`device-power-bench`](../../examples/device-power-bench/) examples. The [`power-save`](../../examples/power-save/) example shows the automatic path.
