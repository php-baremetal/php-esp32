<?php
/**
 * wifi-ap-s3-rgb-manage :: control the onboard RGB LED live, over WebSocket.
 *
 * This is the entry script (runs ONCE): it brings up a WiFi access point, registers the listeners and
 * starts the server; then the resident reactor takes over. The LED state is just a PHP variable that
 * persists between events -- no per-request store needed.
 *
 *   GET  /       -> the control page (rendered by index.php + page.php with the current colour)
 *   GET  /app.js -> the page's script
 *   WS   /ws     -> the page sends {h,s,v,on} as you drag; the board applies it to the LED and
 *                   broadcasts the new state to *every* open browser, so they stay in sync.
 */

use Baremetal\Event;
use Baremetal\Events;
use Baremetal\Http\Request;
use Baremetal\Http\Response;
use Baremetal\Http\Message;

const AP_SSID = 'php-rgb';
const AP_PASS = 'baremetal';   // >= 8 chars for WPA2; '' for an open network

const EFFECTS = ['none', 'disco', 'rainbow', 'pulse', 'strobe'];

final class EffectTick extends Event {}   // the board's animation clock (drives the running effect)

/* ---- the LED state (lives in the resident engine) ---- */
$state = ['h' => 210, 's' => 255, 'v' => 1, 'on' => 1, 'effect' => 'none'];   // calm blue, very dim, no effect

$apply = function () use (&$state): void {
    if ($state['on'] && s3_onboard_rgb_available()) {
        s3_onboard_rgb_hsv($state['h'], $state['s'], $state['v']);
    } else {
        s3_onboard_rgb_off();
    }
};

/* ---- bring up the access point ---- */
if (!wifi_available()) {
    echo "wifi not built -- enable [extensions.wifi] on a WiFi-capable board\n";
    return;
}
if (!wifi_ap_start(AP_SSID, AP_PASS !== '' ? AP_PASS : null)) {
    echo "failed to start the access point\n";
    return;
}
$apply();   // put the LED into the starting colour

$app_js = file_get_contents(__DIR__ . '/app.js');

/* Render the control page: include index.php (the view controller) with the current colour in scope and
 * capture its output. index.php passes the colour on to the page.php template. */
$render = function (array $state): string {
    ob_start();
    include __DIR__ . '/index.php';
    return ob_get_clean();
};

/* ---- HTTP: serve the control page and its script ---- */
Events::listen(Request::class, function (Request $r) use (&$state, $app_js, $render): Response {
    return match ($r->path) {
        '/'       => Response::html($render($state)),
        '/app.js' => new Response(200, $app_js, 'application/javascript'),
        default   => Response::notFound(),
    };
});

/* ---- WebSocket: a frame may carry a manual colour {h,s,v,on} and/or an effect {effect:…}. The colour
 * is always applied; a bare colour (no effect key) stops any running effect. (The page may send the
 * whole state at once, colour + effect together.) ---- */
Events::listen(Message::class, function (Message $m) use (&$state, $apply): void {
    $d = json_decode($m->text, true);
    if (!is_array($d)) {
        return;
    }
    $colour = false;
    foreach (['h' => 359, 's' => 255, 'v' => 255, 'on' => 1] as $k => $hi) {
        if (isset($d[$k])) {
            $state[$k] = max(0, min($hi, (int) $d[$k]));
            $colour = true;
        }
    }
    if (isset($d['effect'])) {
        $state['effect'] = in_array($d['effect'], EFFECTS, true) ? $d['effect'] : 'none';
    } elseif ($colour) {
        $state['effect'] = 'none';   // a bare colour change stops any running effect
    }
    $apply();
    ws_broadcast(json_encode($state));   // reflect the change on every open browser
});

/* ---- the running effect: a board-side timer animates the LED and broadcasts each frame, so it keeps
 * going with no browser open and every client stays in sync ---- */
$tick = 0;
Events::listen(EffectTick::class, function () use (&$state, &$tick, $apply): void {
    if ($state['effect'] === 'none' || !$state['on']) {
        return;
    }
    $tick++;
    switch ($state['effect']) {
        case 'disco':                                  // a new random colour a few times a second
            if ($tick % 2) { return; }
            $state['h'] = random_int(0, 359);
            $state['s'] = 255;
            break;
        case 'rainbow':                                // sweep the hue
            $state['h'] = ($state['h'] + 6) % 360;
            $state['s'] = 255;
            break;
        case 'pulse':                                  // breathe the brightness
            $state['v'] = (int) (8 + 60 * (0.5 - 0.5 * cos($tick / 7)));
            break;
        case 'strobe':                                 // hard on/off
            $state['v'] = ($tick % 2) ? 0 : 80;
            break;
    }
    $apply();
    ws_broadcast(json_encode($state));
});
every(120, EffectTick::class);

serve_ws('/ws');
serve_http(80);

printf("RGB control up: join '%s' (pw '%s') and open http://%s/\n", AP_SSID, AP_PASS, wifi_ap_ip());
