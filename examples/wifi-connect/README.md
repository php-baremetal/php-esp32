# wifi-connect

Scan for WiFi networks and join one, from PHP, on any **WiFi-capable board** (ESP32 / ESP32-S3 /
C-series — not the radio-less ESP32-P4).

The **scan works out of the box** — it lists the access points around you. To **connect**, put your
credentials in a `.env`:

```
cp .env.example .env
# edit .env: WIFI_SSID / WIFI_PASSWORD
```

The `.env` is baked into the firmware at build time and read by the script as `$_ENV` — so your
credentials stay out of the source and out of git (`.env` is gitignored; only `.env.example` is
committed). Leave `WIFI_PASSWORD` empty for an open network.

## The API it uses

- `wifi_scan(): array` — `[{ssid, bssid, rssi, channel, auth}, ...]`
- `wifi_connect(string $ssid, ?string $password = null, int $timeout_ms = 15000): bool`
- `wifi_connected(): bool`, `wifi_ip(): ?string`, `wifi_rssi(): ?int`

## Build & flash

```
phpflash build
phpflash flash
phpflash monitor
```

The serial log lists the networks it found, then the connection result, then a status line every few
seconds.

## Note

The `wifi` extension is **opt-in** (`[extensions.wifi] enabled = true`) because the WiFi stack is
heavy — about 600 KB of flash. On the 4 MB ESP32-S3-Zero the firmware is ~3.25 MB, so it still fits
(with ~200 KB of headroom in the default `factory` partition).
