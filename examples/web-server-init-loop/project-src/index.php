<?php
// A tiny HTTP server, in pure PHP, served over the board's wired Ethernet -- in the
// setup()/loop() model. setup() opens the listening socket once; loop() handles at most one
// request per tick and returns, so the runtime keeps doing its housekeeping (GC, heap checks)
// between connections instead of the script blocking forever in a while-loop.
//
// The firmware brings the network up at boot and logs the address (look for
// "network up -- http://<ip>/" on the serial console). No extension is needed -- PHP's stream
// socket server is part of the core, running straight on the chip's lwIP stack.

$server = null;   // the listening socket, opened in setup()
$hits   = 0;      // requests served
$t0     = 0;      // boot time (seconds), for the uptime counter

function setup(): void
{
    global $server, $t0;
    $addr = 'tcp://0.0.0.0:80';
    $server = @stream_socket_server($addr, $errno, $errstr);
    if ($server === false) {
        echo "could not listen on $addr: $errstr ($errno)\n";
        return;
    }
    // hrtime(false) returns [seconds, nanoseconds]; we only keep the seconds, so the 32-bit
    // integers on this build never overflow (see the language-tour example for why).
    $t0 = hrtime(false)[0];
    echo "HTTP server listening on $addr\n";
}

function loop(int $tick): void
{
    global $server, $hits, $t0;

    if (!$server) {
        delay(1000);   // no socket to serve; don't spin
        return;
    }

    // Wait up to 1s for a client, then hand control back to the runtime. A new connection
    // wakes this immediately, so waiting doesn't cost responsiveness.
    $conn = @stream_socket_accept($server, 1);
    if ($conn === false) {
        return;   // nothing this tick
    }

    $req  = fread($conn, 2048);
    $line = strtok($req, "\r\n");
    if ($line === false) {
        $line = '(no request line)';
    }
    $hits++;
    $uptime = hrtime(false)[0] - $t0;

    $html = "<!doctype html>\n"
          . "<html><head><meta charset=\"utf-8\"><title>PHP on ESP32-P4</title></head>\n"
          . "<body style=\"font-family:sans-serif;max-width:34rem;margin:3rem auto\">\n"
          . "<h1>Hello from PHP " . PHP_VERSION . "</h1>\n"
          . "<p>This page is served by a PHP script running on an ESP32-P4"
          . " microcontroller, over its wired Ethernet.</p>\n"
          . "<ul>\n"
          . "  <li>request #$hits</li>\n"
          . "  <li>uptime: {$uptime}s</li>\n"
          . "  <li>your request line: <code>" . htmlspecialchars($line) . "</code></li>\n"
          . "</ul>\n"
          . "</body></html>\n";

    $resp = "HTTP/1.1 200 OK\r\n"
          . "Content-Type: text/html; charset=utf-8\r\n"
          . "Content-Length: " . strlen($html) . "\r\n"
          . "Connection: close\r\n"
          . "\r\n"
          . $html;

    fwrite($conn, $resp);
    fclose($conn);
    echo "served #$hits: $line\n";
}
