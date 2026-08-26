---
eyebrow: 'Docs · Recipes'
lede: 'Fetch an HTTPS URL from PHP on the chip over a certificate-verified TLS connection, using the full openssl build, a shipped CA bundle, and static DNS on a networked board.'
see_also:
  - href: ../extensions/openssl.md
    meta: 'Extensions'
    label: 'The openssl extension'
  - href: ./persist-state.md
    meta: 'Recipes'
    label: 'Persist and share state'
  - href: ../getting-started/quick-start.md
    meta: 'Getting started'
    label: 'Quick start'
prev:
  label: 'Query SQLite with PDO'
  href: ./sqlite-pdo.md
next:
  label: 'Persist and share state'
  href: ./persist-state.md
---

# Fetch an HTTPS URL

PHP's normal stream layer reaches the internet from the microcontroller. With the full `openssl` build and its `tls` setting, `file_get_contents('https://…')` does DNS, TCP, and a certificate-verified TLS handshake in one call — the same code you would write on a server, running on a 32-bit chip.

## Goal

Fetch an HTTPS URL from PHP over a TLS connection whose peer certificate is verified against a shipped root-CA bundle, and print the response status and body.

## What you need

- A networked board with a cable on a network that has DHCP and internet. This recipe uses `esp32-p4-eth`.
- The full openssl build (`full = true`) with the `tls` setting. The plain subset and a full build without `tls` have no TLS transport.
- A CA bundle on the device. `phpflash build` copies your host's root-CA store into the project so it ships with the firmware.
- Static DNS, or the resolvers DHCP hands out. Name resolution rides the board's network.

## Setup

<!-- @steps -->

1. Enable the full openssl build with the TLS transport in `php-esp32.config.toml` under `[extensions.openssl]`: `enabled = true`, `full = true`, `tls = true`. The `tls` setting builds the `ssl://` / `tls://` stream transport (esp-tls), so `https://` works.
2. Run `phpflash build` (or `phpflash update-certs`) to copy the host trust store into `project-src/certs/ca-bundle.crt`. The firmware points the TLS transport at it and verifies peers against it.
3. Set `[network] dns` to pin your own resolvers, or leave it out to use the DHCP-provided DNS.

<!-- @endsteps -->

<!-- @callout variant="warning" title="Certificates must be provisioned" -->
OpenSSL 3.0's TLS verifies peers, so the device needs a CA bundle. `phpflash build` ships one on the first build and then never overwrites it, so a rebuild cannot silently change what your device trusts. Without a bundle the transport still connects but does **not** verify the peer (it logs a warning) — fine for a quick test, not for production. To pull in renewed roots later, run `phpflash update-certs`, which re-copies the host trust store into `certs_path`.
<!-- @endcallout -->

## The code

`index.php` performs a plain HTTPS GET. The `http` wrapper populates `$http_response_header` with the status line and headers; the peer certificate is verified against the shipped bundle before any bytes flow.

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
// A real HTTPS client on the microcontroller. The full openssl build (full = true) with the
// tls setting compiles a ssl://tls:// stream transport backed by ESP-IDF's esp-tls/mbedTLS,
// so PHP's normal stream layer reaches HTTPS: DNS + TCP + a certificate-verified handshake.

echo "CA bundle: ", getenv('PHP_TLS_CAFILE') ?: '(none -- peers NOT verified)', "\n\n";

$t0   = microtime(true);
$ctx  = stream_context_create(['http' => ['timeout' => 20, 'ignore_errors' => true]]);
$body = @file_get_contents('https://example.com/', false, $ctx);
$ms   = (int) round((microtime(true) - $t0) * 1000);

if ($body === false) {
    echo "GET https://example.com/ FAILED\n";
    echo "  ", (error_get_last()['message'] ?? 'unknown error'), "\n";
} else {
    echo "GET https://example.com/ -> ", ($http_response_header[0] ?? '?'), " in {$ms} ms\n";
    echo "  ", strlen($body), " bytes";
    if (preg_match('~<title>(.*?)</title>~is', $body, $m)) {
        echo ", <title>", trim($m[1]), "</title>";
    }
    echo "\n";
}
echo "--- end ---\n";
```
<!-- @endcode-block -->

Swap `example.com` for your endpoint. For a JSON API, `json_decode(file_get_contents($url))` works as usual. A raw socket is available too: `stream_socket_client('tls://api.example.com:443')`.

<!-- @callout variant="info" title="TLS verifies against the bundle" -->
The transport reads the shipped `ca-bundle.crt` and validates the peer's certificate chain against it. The board logs `loaded CA bundle …` at boot and `TLS connected to <host>:443` once the handshake completes. This is the crypto library's transport (esp-tls/mbedTLS), not OpenSSL's own `libssl`, so a few OpenSSL-specific stream-context options (`peer_fingerprint`, `capture_peer_cert`) are not honored; verified `https://` and `tls://` sockets are what work.
<!-- @endcallout -->

## Config

An embedded-storage `init-loop` project on a networked board: the source, the CA bundle, and the `openssl.cnf` are all flashed into the image, so no card is needed.

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
name = "https-client"
storage_type = "embedded"   # source and the CA bundle are flashed into the image
type = "init-loop"          # execution model

[board]
target = "esp32-p4-eth"     # a board with a network -- TLS/HTTPS need one
port   = ""                 # empty = autodetect at flash time

[network]
dns = ["1.1.1.1", "8.8.8.8"]   # static DNS; comment out to use whatever DHCP hands out

[extensions.openssl]
enabled = true
full    = true
tls     = true              # build the ssl://tls:// client transport -> https:// works
# certs_path   = "certs/ca-bundle.crt"                  # where the CA bundle ships (default)
# certs_source = "/etc/ssl/certs/ca-certificates.crt"   # host bundle to copy (auto-detected)

[php]
src   = "project-src"       # PHP source folder (built into the embedded image)
entry = "index.php"
```
<!-- @endcode-block -->

<!-- @callout variant="note" title="DNS is static" -->
The esp-tls transport resolves names over the board's network. There is no separate DHCP-DNS lookup path unless you rely on the resolvers DHCP hands out — set `[network] dns` to pin your own, and phpflash passes them to the firmware as `-DPHP_NET_DNS=1.1.1.1,8.8.8.8` (comma-separated). Leave the array empty or omit it to keep the DHCP-provided DNS.
<!-- @endcallout -->

## Build & flash

<!-- @code-block language="bash" label="Build, flash, and watch the serial log" -->
```bash
phpflash build && phpflash flash && phpflash monitor
```
<!-- @endcode-block -->

The first full-openssl build cross-compiles OpenSSL 3.0's libcrypto for the chip (a few minutes, cached afterwards). `phpflash build` also copies the CA bundle for you.

## What you'll see

The board brings Ethernet up at boot, applies the static DNS, loads the CA bundle, and runs `index.php`. From a real run on ESP32-P4-ETH:

<!-- @code-block language="text" label="Serial output" -->
```
network up -- http://10.42.0.224/
static DNS[0] = 1.1.1.1
static DNS[1] = 8.8.8.8
PHP 8.3.32 on ESP32-P4
--- /app/index.php ---
CA bundle: /app/certs/ca-bundle.crt

php-tls: loaded CA bundle /app/certs/ca-bundle.crt (223752 bytes)
php-tls: TLS connected to example.com:443
GET https://example.com/ -> HTTP/1.1 200 OK in 11416 ms
  559 bytes, <title>Example Domain</title>
--- end ---
```
<!-- @endcode-block -->

A handshake takes around 10 s: the base config uses software AES (the AES accelerator's DMA cannot reach the PSRAM the PHP heap lives in) and the RSA cert-chain verify is software too. That is fine for occasional requests, and the task-watchdog window covers it.

The full example lives in [`examples/https-client/`](https://github.com/php-baremetal/php-esp32/tree/master/examples/https-client).
