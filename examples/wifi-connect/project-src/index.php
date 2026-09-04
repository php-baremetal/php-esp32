<?php
// Scan for WiFi networks and join one, from PHP.
//
// The scan works out of the box (it lists the APs around you). To actually connect, copy .env.example
// to .env and set your network's SSID/password there -- the .env is baked into the firmware and read
// here as $_ENV, kept out of the source and out of git. Needs the `wifi` extension (WiFi-capable SoCs).

$SSID = $_ENV['WIFI_SSID'] ?? '';
$PASS = $_ENV['WIFI_PASSWORD'] ?? '';   // empty for an open network

function setup(): void
{
    global $SSID, $PASS;

    if (!wifi_available()) {
        echo "wifi not built -- enable [extensions.wifi] on a WiFi-capable board\n";
        return;
    }

    echo "scanning for networks...\n";
    $aps = wifi_scan();   // array of APs, or false if the scan itself failed
    if ($aps === false) {
        echo "  scan failed\n";
    }
    foreach ($aps ?: [] as $ap) {
        printf("  %-32s ch%-3d %4d dBm  %s\n",
               $ap['ssid'] !== '' ? $ap['ssid'] : '(hidden)',
               $ap['channel'], $ap['rssi'], $ap['auth']);
    }

    if ($SSID === '') {
        echo "\nno WIFI_SSID set -- copy .env.example to .env and fill it in to connect\n";
        return;
    }

    echo "\nconnecting to '" . $SSID . "'...\n";
    if (wifi_connect($SSID, $PASS !== '' ? $PASS : null)) {
        echo "connected! IP " . wifi_ip() . "  (" . wifi_rssi() . " dBm)\n";
    } else {
        echo "connect failed -- check the credentials in .env\n";
    }
}

function loop(int $tick): void
{
    if (wifi_connected()) {
        echo "tick $tick: online, IP " . wifi_ip() . ", rssi " . wifi_rssi() . " dBm\n";
    } else {
        echo "tick $tick: offline\n";
    }
    delay(5000);
}
