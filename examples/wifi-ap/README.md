# wifi-ap

Turn the board into a **WiFi access point** from PHP — it creates its own network, with its own DHCP
server, that you connect to. Works on any **WiFi-capable board** (ESP32 / ESP32-S3 / C-series — not
the radio-less ESP32-P4).

Set the network name/password at the top of `index.php` (default `php-esp32` / `baremetal`, WPA2),
build and flash. Then connect a phone or laptop to it — the board hands out an IP and lives at
`192.168.4.1`.

## The API it uses

- `wifi_ap_start(string $ssid, ?string $password = null, int $channel = 1, int $max_conn = 4): bool`
  (password `null`/`""` = open; WPA2 needs ≥ 8 characters)
- `wifi_ap_ip(): ?string`, `wifi_ap_clients(): int`, `wifi_ap_stop(): bool`

## Build & flash

```
phpflash build
phpflash flash
phpflash monitor
```

The serial log prints the AP address, then a count of connected devices every few seconds — watch it
go up as you join.

## Why this is neat

No router, no infrastructure: **the board *is* the network**. Pair this with the `web-server` model
and your PHP can serve a page straight to whoever joins — a standalone web server on a $4 chip.
The [`wifi-ap-s3-rgb-manage`](../wifi-ap-s3-rgb-manage/) example does exactly that: it brings the AP up
in its server_init script, then serves a live PHP page that controls the onboard RGB LED.
