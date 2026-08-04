<?php
// A real HTTPS client on the microcontroller. The full openssl build (`full = true`) with the
// `tls` setting compiles a ssl://tls:// stream transport backed by ESP-IDF's esp-tls/mbedTLS, so
// PHP's normal stream layer can reach HTTPS: DNS + TCP + a certificate-verified TLS handshake.
// The board (esp32-p4-eth) brings the network up at boot; phpflash ships the host's root CAs.

echo "CA bundle: ", getenv('PHP_TLS_CAFILE') ?: '(none -- peers NOT verified)', "\n\n";

// A plain HTTPS GET. The peer certificate is verified against the shipped CA bundle.
$t0   = microtime(true);
$ctx  = stream_context_create(['http' => ['timeout' => 20, 'ignore_errors' => true]]);
$body = @file_get_contents('https://example.com/', false, $ctx);
$ms   = (int) round((microtime(true) - $t0) * 1000);

if ($body === false) {
    echo "GET https://example.com/ FAILED\n";
    echo "  ", (error_get_last()['message'] ?? 'unknown error'), "\n";
} else {
    // $http_response_header is populated by the http wrapper with the status + headers.
    echo "GET https://example.com/ -> ", ($http_response_header[0] ?? '?'), " in {$ms} ms\n";
    echo "  ", strlen($body), " bytes";
    if (preg_match('~<title>(.*?)</title>~is', $body, $m)) {
        echo ", <title>", trim($m[1]), "</title>";
    }
    echo "\n";
}
echo "--- end ---\n";
