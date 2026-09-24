<?php
/**
 * HTTP in the event-driven model: an `http` source. No loop() -- the board creates its own WiFi access
 * point, starts an HTTP server, and each request arrives at the reactor as a Baremetal\Http\Request
 * event. A listener returns a Baremetal\Http\Response, routed back to the same socket. Routing is
 * userland (a match), exactly as a framework would do it -- the firmware ships no router.
 *
 * Join the access point below, then open http://<ap-ip>/ (the ip is printed at boot).
 */

use Baremetal\Events;
use Baremetal\Http\Request;
use Baremetal\Http\Response;

const AP_SSID = 'php-esp32';
const AP_PASS = 'baremetal';   // >= 8 chars for WPA2; '' for an open network

if (!wifi_available()) {
    echo "wifi not built -- enable [extensions.wifi] on a WiFi-capable board\n";
    return;
}

if (!wifi_ap_start(AP_SSID, AP_PASS !== '' ? AP_PASS : null)) {
    echo "failed to start the access point\n";
    return;
}

$requests = 0;

Events::listen(Request::class, function (Request $r) use (&$requests): Response {
    $requests++;
    return match (true) {
        $r->method === 'GET'  && $r->path === '/' => Response::html(
            "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
            . "<h1>php-esp32</h1><p>event-driven HTTP source.</p>"
            . "<p>requests served: {$requests}</p>"
            . "<ul><li>GET <code>/status</code> (JSON)</li>"
            . "<li>POST <code>/echo</code> (JSON body echoed back)</li></ul>"
        ),
        $r->method === 'GET'  && $r->path === '/status' => Response::json([
            'requests'   => $requests,
            'ap_clients' => wifi_ap_clients(),
            'ssid'       => AP_SSID,
        ]),
        $r->method === 'POST' && $r->path === '/echo' => Response::json([
            'you_sent' => $r->json(),
        ]),
        default => Response::notFound(),
    };
});

serve_http(80);

printf("AP '%s' up at %s -- join it and open http://%s/\n", AP_SSID, wifi_ap_ip(), wifi_ap_ip());
