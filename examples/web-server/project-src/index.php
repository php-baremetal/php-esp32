<?php
// This runs fresh for every HTTP request, behind the firmware's HTTP server -- the `web-server`
// project type, shared-nothing the way a script runs under Apache / PHP-FPM. You don't manage
// the socket or a loop: just produce the page, and whatever you echo becomes the response body.
// The firmware fills a minimal $_SERVER (method + URI) before running this.

$uri    = $_SERVER['REQUEST_URI']    ?? '/';
$method = $_SERVER['REQUEST_METHOD'] ?? 'GET';

echo "<!doctype html>\n";
echo "<html><head><meta charset=\"utf-8\"><title>PHP on ESP32-P4</title></head>\n";
echo "<body style=\"font-family:sans-serif;max-width:34rem;margin:3rem auto\">\n";
echo "<h1>Hello from PHP " . PHP_VERSION . "</h1>\n";
echo "<p>This page ran <em>fresh</em> for your request, behind an HTTP server on an"
   . " ESP32-P4 microcontroller.</p>\n";
echo "<ul>\n";
echo "  <li>you requested: <code>" . htmlspecialchars("$method $uri") . "</code></li>\n";
// Each request is a clean PHP run, so there's no counter to keep -- a random number shows
// that this really executed again rather than being cached.
echo "  <li>a fresh random number: " . random_int(1000, 9999) . "</li>\n";
echo "</ul>\n";
echo "</body></html>\n";
