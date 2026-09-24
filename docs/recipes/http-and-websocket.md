---
eyebrow: 'Docs · Recipes'
lede:    'Serve HTTP and WebSocket from the event-driven reactor: serve_http() turns each request into a Request event you answer with a Response, serve_ws() turns each frame into a Message, and ws_broadcast() pushes to every client — a REST API and live push on one resident engine.'
see_also:
  - { href: './event-driven.md', meta: '8 min' }
  - { href: './web-page.md', meta: '8 min' }
  - { href: '../getting-started/execution-models.md', meta: '6 min' }
prev: { label: 'Event-driven basics', href: './event-driven.md' }
next: { label: 'Query SQLite with PDO', href: './sqlite-pdo.md' }
---

# HTTP & WebSocket (event-driven)

In the `event-driven` model, HTTP requests and WebSocket frames are just more sources feeding the
reactor. This is different from the [`web-server`](./web-page.md) model: here the engine is **resident**
(state persists between requests), and the same reactor also handles timers, GPIO and sensors.

Enable the extension and start the server during setup:

```toml
type = "event-driven"

[extensions.web]
enabled = true

[extensions.wifi]     # a SoftAP, if the board has no wired network
enabled = true
```

> The board needs a network. A wired `-ETH` board brings the link up at boot; on a WiFi board call
> `wifi_ap_start('my-ap', 'password')` (or `wifi_connect(...)`) during setup before `serve_http()`.

## HTTP: Request → Response

`serve_http($port)` starts the server. Each request arrives as a `Baremetal\Http\Request` event; the
first listener that returns a `Baremetal\Http\Response` answers it, routed back to the same socket.
Routing is yours — a `match` on method and path (the firmware ships no router).

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
use Baremetal\Events;
use Baremetal\Http\{Request, Response};

$state = ['count' => 0];

Events::listen(Request::class, function (Request $r) use (&$state): Response {
    return match (true) {
        $r->method === 'GET'  && $r->path === '/'       => Response::html('<h1>hi from PHP</h1>'),
        $r->method === 'GET'  && $r->path === '/status' => Response::json(['count' => $state['count']]),
        $r->method === 'POST' && $r->path === '/bump'   => Response::json(['count' => ++$state['count']]),
        default => Response::notFound(),
    };
});

serve_http(80);
```

- **Exactly one handler answers** — the first `Response` wins (like returning `false`, it stops
  propagation). No `Response` → `404`. A handler that throws → `500` (the socket is never left hanging).
- **`Request`**: `$r->method`, `$r->path`, `$r->query`, `$r->body`, `$r->ip`, `$r->headers` (array),
  plus `$r->json()` (decode the body) and `$r->header($name)`.
- **`Response`**: `Response::ok($body)`, `::text()`, `::html()`, `::json($data)`, `::notFound()`,
  `::noContent()`, or `new Response($status, $body, $contentType)`.
- The classic superglobals **`$_GET` / `$_POST` / `$_REQUEST` / `$_SERVER`** are populated per request
  too, so existing request code works (a JSON body goes to `$r->json()`, not `$_POST`, as in standard PHP).
- Because the engine is resident, a variable like `$state` above **persists across requests**.

## WebSocket: Message and broadcast

`serve_ws($path)` adds a WebSocket endpoint on the same server. Each inbound frame is a
`Baremetal\Http\Message` event; `$m->reply()` answers that client, and `ws_broadcast()` pushes to all.

```php
use Baremetal\Http\Message;

Events::listen(Message::class, function (Message $m): void {
    $m->reply('echo: ' . $m->text);          // ->text, ->client, ->binary, ->reply()
});

serve_ws('/ws');     // call before serve_http() so /ws wins over the catch-all
serve_http(80);
```

Unsolicited push — the streaming half — is `ws_broadcast($data)`: send a frame to **every** connected
client (returns how many it reached). Drive it from any event, e.g. a timer or a sensor:

```php
final class Beat extends Baremetal\Event {}
$n = 0;
Events::listen(Beat::class, function () use (&$n) { ws_broadcast(json_encode(['beat' => ++$n])); });
every(1000, Beat::class);
```

On the client:

```js
const ws = new WebSocket('ws://' + location.host + '/ws');
ws.onmessage = e => console.log(JSON.parse(e.data));
ws.send(JSON.stringify({ hello: 'board' }));
```

**Backpressure (inbound):** if events arrive faster than PHP drains them, the server pauses reading the
socket (TCP slows the sender) rather than dropping frames — inbound commands are not lost.

**Outbound is non-blocking:** `ws_broadcast()` sends only to clients whose socket is ready to accept the
frame; a slow client is skipped for that frame rather than stalling the reactor. So a live stream stays
smooth for everyone even if one client falls behind (keep the broadcast rate modest — ~30/s is plenty —
since every frame shares the one reactor queue with your HTTP handlers).

## Put together

A sensor polled on core 1, broadcast live over WebSocket, with the page served over HTTP — all on one
reactor — is the [`imu-ws-stream`](../../examples/imu-ws-stream/) example. A REST + WebSocket control
page driving the onboard LED is [`wifi-ap-s3-rgb-manage`](../../examples/wifi-ap-s3-rgb-manage/).
