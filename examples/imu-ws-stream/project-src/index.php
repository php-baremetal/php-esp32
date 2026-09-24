<?php
/**
 * imu-ws-stream :: IMU polled on core 1, broadcast to the browser over WebSocket, page served over HTTP
 * -- all on one event-driven reactor. Join the SoftAP below and open http://192.168.4.1/.
 */

use Baremetal\Events;
use Baremetal\Event;
use Baremetal\Http\Request;
use Baremetal\Http\Response;
use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Qmi8658;

const AP_SSID = 'php-imu';
const AP_PASS = 'baremetal';

final class StreamTick extends Event {}   // the broadcast clock (fixed rate, decoupled from the poll)

if (!wifi_available()) {
    echo "wifi not built -- enable [extensions.wifi]\n";
    return;
}
if (!wifi_ap_start(AP_SSID, AP_PASS)) {
    echo "failed to start the access point\n";
    return;
}

$imu = new Qmi8658(new Bus(sda: 11, scl: 10));
$imu->poll(hz: 50, depth: 8);   // core-1 sampling into a ring

// A 20 Hz timer broadcasts the latest sample -- decoupled from the poll keeps the stream smooth.
$temp = 0.0;
$k = 0;
Events::listen(StreamTick::class, function () use ($imu, &$temp, &$k): void {
    $raw = $imu->sample();
    if ($raw === null) {
        return;
    }
    ['accel' => [$ax, $ay, $az], 'gyro' => [$gx, $gy, $gz]] = $imu->decode($raw);
    if (($k++ % 4) === 0) {
        $temp = $imu->temp();   // temp changes slowly
    }
    ws_broadcast(json_encode([
        'ax' => round($ax, 3), 'ay' => round($ay, 3), 'az' => round($az, 3),
        'gx' => round($gx, 1), 'gy' => round($gy, 1), 'gz' => round($gz, 1),
        't'  => round($temp, 1),
    ]));
});
every(50, StreamTick::class);

$page   = file_get_contents(__DIR__ . '/page.html');
$app_js = file_get_contents(__DIR__ . '/app.js');

Events::listen(Request::class, function (Request $r) use ($page, $app_js): Response {
    return match ($r->path) {
        '/'       => Response::html($page),
        '/app.js' => new Response(200, $app_js, 'application/javascript'),
        default   => Response::notFound(),
    };
});

serve_ws('/ws');
serve_http(80);

printf("imu-ws-stream: join '%s' (pw '%s') and open http://%s/\n", AP_SSID, AP_PASS, wifi_ap_ip());
