<?php
/**
 * Full HTTP test for the event-driven `http` source, over wired Ethernet (no WiFi). One handler reflects
 * everything it received -- method, path, GET params (the query string), POST/body params (JSON or
 * form-urlencoded), a couple of headers and the client IP -- so every method and every parameter style
 * can be exercised with curl. The link comes up at boot; watch the serial for "network up -- http://<ip>/".
 *
 *   curl http://<ip>/whoami
 *   curl 'http://<ip>/get?name=ada&n=42'
 *   curl -X POST http://<ip>/form -d 'a=1&b=two'
 *   curl -X POST http://<ip>/json -H 'Content-Type: application/json' -d '{"x":[1,2,3]}'
 *   curl -X PUT    http://<ip>/item/7 -d 'v=updated'
 *   curl -X PATCH  http://<ip>/item/7 -H 'Content-Type: application/json' -d '{"v":9}'
 *   curl -X DELETE 'http://<ip>/item?id=7'
 *   curl -i -X OPTIONS http://<ip>/anything
 *   curl -I http://<ip>/                       # HEAD
 *   curl -i http://<ip>/boom                   # a throwing handler -> 500
 */

use Baremetal\Event;
use Baremetal\Events;
use Baremetal\Http\Request;
use Baremetal\Http\Response;
use Baremetal\Http\Message;

final class Tick extends Event {}

/** Parse the request body into an array/value according to its Content-Type. */
function parse_body(Request $r): mixed
{
    $ctype = strtolower((string) ($r->header('Content-Type') ?? ''));
    if ($r->body === '') {
        return null;
    }
    if (str_contains($ctype, 'application/json')) {
        return $r->json();
    }
    if (str_contains($ctype, 'application/x-www-form-urlencoded')) {
        parse_str($r->body, $form);
        return $form;
    }
    return $r->body;   // raw (text/plain, etc.)
}

$requests = 0;

Events::listen(Request::class, function (Request $r) use (&$requests): Response {
    $requests++;

    // A landing page and the two deliberate special cases.
    if ($r->method === 'GET' && $r->path === '/') {
        return Response::html(
            "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
            . "<h1>php-esp32 full HTTP test</h1><p>requests served: {$requests}</p>"
            . "<p>Every other path reflects what it received as JSON. Try any method.</p>"
        );
    }
    if ($r->path === '/boom') {
        throw new RuntimeException('kaboom');   // -> 500, and the server keeps serving
    }

    // Parse GET params (the query string) and POST/body params (by Content-Type).
    parse_str($r->query, $get);

    return Response::json([
        'ok'         => true,
        'seq'        => $requests,
        'method'     => $r->method,
        'path'       => $r->path,
        // via the Request object:
        'query'      => $get,               // parameters passed in the URL (GET style)
        'body'       => parse_body($r),     // parameters passed in the body (POST/PUT/PATCH)
        'body_raw'   => $r->body,
        'client_ip'  => $r->ip,
        'user_agent' => $r->header('User-Agent'),
        // via the classic superglobals (populated per request even though the engine is resident):
        '_GET'       => $_GET,
        '_POST'      => $_POST,
        '_REQUEST'   => $_REQUEST,
        '_SERVER'    => [
            'REQUEST_METHOD' => $_SERVER['REQUEST_METHOD'] ?? null,
            'REQUEST_URI'    => $_SERVER['REQUEST_URI'] ?? null,
            'QUERY_STRING'   => $_SERVER['QUERY_STRING'] ?? null,
            'REMOTE_ADDR'    => $_SERVER['REMOTE_ADDR'] ?? null,
            'CONTENT_TYPE'   => $_SERVER['CONTENT_TYPE'] ?? null,
            'HTTP_USER_AGENT'=> $_SERVER['HTTP_USER_AGENT'] ?? null,
        ],
    ]);
});

// WebSocket source: each inbound frame is a Message event; here we echo it back to the sender and
// count frames per client. `serve_ws()` is called before `serve_http()` so the /ws route wins over the
// catch-all.
$frames = 0;
Events::listen(Message::class, function (Message $m) use (&$frames): void {
    $frames++;
    $decoded = json_decode($m->text, true);
    $reply = ($decoded !== null)
        ? json_encode(['echo' => $decoded, 'frame' => $frames, 'client' => $m->client])
        : sprintf('echo #%d from client %d: %s', $frames, $m->client, $m->text);
    $m->reply($reply);
});

// Unsolicited push: a timer broadcasts a heartbeat to every connected WebSocket client (ws_broadcast),
// the streaming half of the model -- nobody asked for it.
$beat = 0;
Events::listen(Tick::class, function () use (&$beat): void {
    ws_broadcast(json_encode(['beat' => ++$beat]));
});
every(1000, Tick::class);

serve_ws('/ws');
serve_http(80);

echo "full HTTP + WS test ready on :80 (WS at /ws, heartbeat broadcast every 1s)\n";
