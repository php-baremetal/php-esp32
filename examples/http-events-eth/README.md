# http-events-eth

A full HTTP **and** WebSocket test on a **wired** board (ESP32-P4-ETH): like
[`http-events`](../http-events/) but over Ethernet instead of a WiFi SoftAP. The firmware brings the link
up at boot and logs `network up -- http://<ip>/`; the script starts the server, routes `Request` events
(every method, params via query and body) and echoes WebSocket frames — no WiFi.

```php
use Baremetal\Events;
use Baremetal\Http\Request;
use Baremetal\Http\Response;

Events::listen(Request::class, function (Request $r): Response {
    return match (true) {
        $r->method === 'GET'  && $r->path === '/status' => Response::json(['ok' => true]),
        $r->method === 'POST' && $r->path === '/echo'   => Response::json(['you_sent' => $r->json()]),
        $r->method === 'GET'  && $r->path === '/boom'   => throw new RuntimeException('kaboom'), // -> 500
        default => Response::notFound(),                                                          // -> 404
    };
});

serve_http(80);
```

Parameters are available both ways: on the `Request` (`$r->query` + `parse_str`, `$r->json()`,
`$r->body`, `$r->header()`) **and** through the classic superglobals `$_GET`, `$_POST`, `$_REQUEST`,
`$_SERVER`, which the http source populates per request (a JSON body goes to `$r->json()`, not `$_POST`,
as in standard PHP).

## WebSocket

The same server also exposes a WebSocket endpoint at `/ws`. Each inbound frame arrives as a
`Baremetal\Http\Message` event; this example echoes it back:

```php
use Baremetal\Http\Message;

Events::listen(Message::class, function (Message $m): void {
    $m->reply("echo: " . $m->text);          // ->text, ->client, ->binary, ->reply()
});

serve_ws('/ws');   // before serve_http() so the /ws route wins over the catch-all
serve_http(80);
```

`serve_ws()` is called before `serve_http()` so the exact `/ws` route is matched before the HTTP
catch-all. HTTP and WebSocket share one server and one reactor queue.

This example also **broadcasts** a heartbeat to every connected client once a second, driven by a timer:

```php
Events::listen(Tick::class, fn () => ws_broadcast(json_encode(['beat' => ++$beat])));
every(1000, Tick::class);
```

`ws_broadcast($data)` pushes a text frame to all connected WebSocket clients and returns how many it
reached — the streaming half of the model (nobody requested it). Connect a client and you will see the
heartbeat arrive without sending anything.

See [`http-events`](../http-events/) for how the model works (the httpd task parks the request, the
reactor answers with the first `Response` a listener returns, routed back to the same socket).

## Run

```sh
phpflash flash
phpflash monitor      # note the "network up -- http://<ip>/" line
```

From any machine on the same network as the board:

```sh
curl http://<ip>/status
curl -X POST http://<ip>/echo -d '{"hello":"world"}' -H 'Content-Type: application/json'
curl -i http://<ip>/nope     # 404
curl -i http://<ip>/boom     # 500 (the handler threw; the server keeps serving)
```
