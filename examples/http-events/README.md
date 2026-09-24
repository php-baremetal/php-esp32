# http-events

HTTP as an **event source**. In the `event-driven` model there is no `loop()`: the board brings up its
own WiFi access point, starts an HTTP server with `serve_http()`, and each incoming request is delivered
to the reactor as a `Baremetal\Http\Request` event. A listener returns a `Baremetal\Http\Response`,
which is routed back to the same socket.

```php
use Baremetal\Events;
use Baremetal\Http\Request;
use Baremetal\Http\Response;

wifi_ap_start('php-esp32', 'baremetal');

Events::listen(Request::class, function (Request $r): Response {
    return match (true) {
        $r->method === 'GET'  && $r->path === '/status' => Response::json(['ok' => true]),
        $r->method === 'POST' && $r->path === '/echo'   => Response::json(['you_sent' => $r->json()]),
        default => Response::notFound(),
    };
});

serve_http(80);
```

## The model

- **Routing is userland.** The firmware ships no router — a `match` on `$r->method`/`$r->path` is all it
  takes, exactly what a framework does. There is deliberately no `Http` facade verb.
- **Exactly one handler answers.** The first listener that returns a `Response` wins (like `return
  false` stopping propagation); if none returns a `Response`, the client gets `404`; if a handler
  throws, the client gets `500` — the socket is never left hanging.
- **PHP runs on its own stack, not httpd's.** The httpd task parks the request and posts it to the
  reactor on core 0; PHP runs there and hands the `Response` back. One request is in flight at a time.

`Request` exposes `method`, `path`, `query`, `body`, `ip`, `headers` (an array), plus `->json()`
(decodes the body) and `->header($name)`. `Response` has `ok()`, `text()`, `html()`, `json()`,
`notFound()`, `noContent()`, or `new Response($status, $body, $contentType)`.

## Config

`type = "event-driven"`, `[extensions.wifi]` and `[extensions.web]` enabled. Any WiFi-capable board.

## Run

```sh
phpflash flash
phpflash monitor      # prints "AP 'php-esp32' up at <ip>"
```

Join the `php-esp32` access point (password `baremetal`), then open `http://<ip>/`. Try:

```sh
curl http://<ip>/status
curl -X POST http://<ip>/echo -d '{"hello":"world"}' -H 'Content-Type: application/json'
```
