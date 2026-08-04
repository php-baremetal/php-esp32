# https-client

A real **HTTPS client** on the microcontroller: PHP fetches `https://example.com/` over a
certificate-verified TLS connection, on an ESP32-P4-ETH. This is the full `openssl` build with the
`tls` setting — the `ssl://`/`tls://` stream transport, backed by ESP-IDF's esp-tls/mbedTLS, so
PHP's normal stream layer reaches HTTPS (DNS + TCP + TLS in one go).

> Needs a **networked board** (this uses `esp32-p4-eth`) with a cable on a network that has DHCP and
> internet. The crypto is real OpenSSL 3.0 (libcrypto); the TLS transport rides mbedTLS. See
> [`docs/openssl.md`](../../docs/openssl.md) → *TLS client (HTTPS)*.

## What it does

```php
$body = file_get_contents('https://example.com/');   // verified HTTPS GET
preg_match('~<title>(.*?)</title>~is', $body, $m);    // -> "Example Domain"
```

The board brings Ethernet up at boot, applies the static DNS from the config, and the TLS transport
verifies `example.com`'s certificate against the root CAs shipped with the firmware.

## Configuration

`php-esp32.config.toml`:

```toml
[network]
dns = ["1.1.1.1", "8.8.8.8"]   # static DNS (optional; DHCP's are used otherwise)

[extensions.openssl]
enabled = true
full    = true
tls     = true                 # build the TLS client transport
```

**Certificates.** `phpflash build` copies your host's root-CA bundle into
`project-src/certs/ca-bundle.crt` (git-ignored — it's a host artifact) and the firmware verifies TLS
peers against it. Override the destination with `certs_path` or the source with `certs_source`
(`[extensions.openssl]`). Without a bundle the client still connects but does **not** verify the peer.

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

The first full-openssl build cross-compiles OpenSSL (a few minutes, cached). `phpflash build` also
copies the CA bundle for you.

## The output

From [`monitor.txt`](monitor.txt) (a real run on ESP32-P4-ETH):

```
network up -- http://10.42.0.224/
static DNS[0] = 1.1.1.1
static DNS[1] = 8.8.8.8
...
CA bundle: /app/certs/ca-bundle.crt
loaded CA bundle /app/certs/ca-bundle.crt (223752 bytes)
TLS connected to example.com:443
GET https://example.com/ -> HTTP/1.1 200 OK in 11416 ms
  559 bytes, <title>Example Domain</title>
```

A certificate-verified HTTPS request, from PHP, on a 32-bit microcontroller.

## Notes

- **A handshake takes ~10 s.** The base config uses software AES (the AES accelerator's DMA can't
  reach the PSRAM the PHP heap lives in), and the RSA cert-chain verify is software too. Fine for
  occasional requests; the task-watchdog window covers it.
- **Client only.** No TLS server, and a few OpenSSL-specific stream-context options aren't honored
  (the transport is mbedTLS, not `libssl`). Verified `https://` and `tls://` sockets work.
- Swap `example.com` for your endpoint. For a JSON API, `json_decode(file_get_contents($url))` works
  as usual.
