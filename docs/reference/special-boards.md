---
eyebrow: 'Docs · Reference'
lede:    'When an ESP32-P4 board counts as a WiFi board: the radio-less P4 gets WiFi only from a companion chip (an on-board ESP32-C6 over ESP-HOSTED), what such a board needs, and why an integrated companion is very probably pre-flashed and ready out of the box.'
see_also:
  - { href: './porting-notes.md', meta: 'Reference', label: 'Porting notes' }
  - { href: '../getting-started/architecture.md', meta: 'Getting started', label: 'Architecture' }
  - { href: 'https://github.com/espressif/esp-hosted-mcu', meta: 'external', label: 'ESP-Hosted-MCU' }
prev: { label: 'Partitions and the flash layout', href: './partitions.md' }
next: { label: 'No next page', href: '#' }
---

# Special boards

Most boards are ordinary: what they can do is what the main chip's own peripherals do, and the board
profile just wires them up. A **special board** pairs the main chip with an extra component that
changes what the chip can do on its own. Today that means one thing: an **ESP32-P4 with a WiFi
companion**.

## When is an ESP32-P4 board a "WiFi" board?

The ESP32-P4 has **no built-in radio** — a chip probe says so plainly:

```
$ phpflash discover
Chip:   ESP32-P4 (revision v1.3)
Radio:  none (no built-in WiFi/BT; this chip needs a companion for wireless)
```

So a bare P4 board (`esp32-p4-pico`, `esp32-p4-eth`, `esp32-p4-zero`) has no WiFi at all. It becomes a
WiFi board only when it carries a **companion radio chip** — typically an on-board **ESP32-C6** — wired
to the P4 over **SDIO** and running the **ESP-HOSTED** slave firmware. The P4 is the *host*; the C6 is
the *radio*. On the host, `esp_wifi_remote` re-exposes the normal `esp_wifi_*` API and forwards every
call to the companion over the SDIO transport (`esp_hosted`). This is the **ESP32-P4-WIFI6** board
(`esp32-p4-wifi-c6`).

Because the API is identical, **the `wifi` extension and your PHP code are unchanged** — `wifi_scan()`,
`wifi_connect()`, `wifi_ap_start()` all work exactly as on an ESP32-S3. A board profile declares this
capability in its `board.toml` with `network = "wifi"`.

## What such a board needs

Three things have to line up for WiFi to work on a P4:

1. **A companion radio running the ESP-HOSTED slave firmware.** The C6 (or C5/C61) runs a small
   firmware that turns it into a radio the P4 drives over SDIO.
2. **The SDIO wiring the firmware expects.** The default matches the **ESP32-P4-Function-EV-Board**
   reference pin map (host side): `CLK=18`, `CMD=19`, `D0=14`, `D1=15`, `D2=16`, `D3=17`, and a
   `RESET` line to the companion (`GPIO54`). The `esp32-p4-wifi-c6` board's `sdkconfig.board` selects
   this reference mapping. If a board wires the SDIO differently, it sets custom pins instead.
3. **The host components.** `esp_wifi_remote` (the forwarding API) and `esp_hosted` (the SDIO
   transport + implementation). These are ESP-IDF **managed components**: a `phpflash build` for a
   P4 target **downloads them automatically** (declared in `main/idf_component.yml`, gated to the
   `esp32p4` target) — there is no manual install step. They land in `managed_components/` in the
   repo and are git-ignored, like the fetched PHP source.

Nothing in the PHP layer changes; the host `sdkconfig` just selects the C6 slave and the reference
SDIO pins, and the extension gate (`esp32p4` is no longer excluded) lets the `wifi` extension build.

## Out of the box: pre-flashed and connected

**If the WiFi companion is integrated on the board when you buy it, it is very probably already
pre-flashed with the ESP-HOSTED slave firmware and already wired to the P4 over SDIO.** So it works
out of the box: build for `esp32-p4-wifi-c6`, flash the **P4** (only the P4 — not the companion), and
WiFi is there. You do not flash the C6 yourself.

You only need to touch the companion if you wired your own, or if the boot log shows a firmware
**version mismatch** between the host's ESP-HOSTED and the companion — in which case you can
(re)flash / OTA-update the C6 with a matching slave firmware (see
[esp-hosted-mcu](https://github.com/espressif/esp-hosted-mcu)). A mismatch is a warning, not
necessarily a failure — the board can still work.

## Verified

On an **ESP32-P4-WIFI6** (P4 rev v1.3), verified on hardware: PHP brought up a SoftAP
(`wifi_ap_start()`), the C6 answered over SDIO (`slave chip id: 0x0d (esp32c6)`) using the reference
pins with no schematic needed, the companion was **already pre-flashed**, and the board served a page
over its own WiFi:

```
eh_sdio: SDIO 4-bit 40000 kHz CLK=18 CMD=19 D0=14 D1=15 D2=16 D3=17 RESET=54
eh_sdio: Card init success
eh_init_evt: slave chip id: 0x0d (esp32c6)
PHP 8.4.25 on ESP32-P4
access point 'php-esp32' is up at 192.168.4.1
web-server model: serving /app/index.php over HTTP on :80
```

The payoff is bigger than on the S3: the P4 has **32 MB of PSRAM**, so it can serve not just plain
pages but full frameworks (Laravel, Symfony) — now over WiFi, with no wired network at all.
