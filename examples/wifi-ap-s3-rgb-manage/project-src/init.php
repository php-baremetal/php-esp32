<?php
// server_init: runs ONCE, before the HTTP server starts (the [web-server] init hook). Its output
// goes to the serial console. Two jobs: bring up a WiFi access point so the board serves its page
// over WiFi (no router needed), and put the RGB LED into a known starting state.

const AP_SSID = 'php-rgb';
const AP_PASS = 'baremetal';   // >= 8 chars for WPA2; '' (empty) for an open network

if (!wifi_available()) {
    echo "[init] wifi not built -- enable [extensions.wifi]\n";
} elseif (wifi_ap_start(AP_SSID, AP_PASS !== '' ? AP_PASS : null)) {
    $ip = wifi_ap_ip();
    echo "[init] access point '" . AP_SSID . "' up at $ip\n";
    echo "[init] join it" . (AP_PASS !== '' ? " (password '" . AP_PASS . "')" : " (open)") . " then open http://$ip/\n";
} else {
    echo "[init] failed to start the access point\n";
}

// The WS2812 physically holds its colour between requests, so the LED itself is our state. We also
// mirror the numbers in the in-RAM mem_* store so each request can render the current slider values.
$h = 210; $s = 255; $v = 40; $on = 1;   // a calm blue at low brightness to start
mem_set('h', $h);
mem_set('s', $s);
mem_set('v', $v);
mem_set('on', $on);

if (s3_onboard_rgb_available()) {
    s3_onboard_rgb_hsv($h, $s, $v);
    echo "[init] LED ready (h=$h s=$s v=$v)\n";
} else {
    echo "[init] RGB LED not built -- enable [extensions.s3_onboard_rgb]\n";
}
