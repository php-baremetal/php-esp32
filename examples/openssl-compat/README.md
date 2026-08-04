# openssl-compat

AES encryption on the microcontroller with the **compatible** `openssl` extension — the
`openssl_*` functions backed by the chip's mbedTLS. This is the small (~42 KB) subset build:
symmetric ciphers only (`openssl_encrypt` / `openssl_decrypt`, `openssl_cipher_iv_length`,
`openssl_random_pseudo_bytes`), which is enough for the common "encrypt a blob with a shared key"
case — including a framework's encrypter (e.g. Laravel's).

> There are two openssl examples. This one uses the mbedTLS **subset** (symmetric AES, tiny). The
> sibling [`openssl-full`](../openssl-full/) uses the real ported OpenSSL (public-key crypto,
> ~2 MB). See [`docs/openssl.md`](../../docs/openssl.md) for when to pick which.

## Firmware

`ext/openssl` is optional and off by default. This project's `php-esp32.config.toml` enables the
subset (`[extensions.openssl]`, `full = false`), so `phpflash build` compiles it in with no flags
to pass. It needs no board in particular — mbedTLS is already on the chip.

## What it does

```php
$key = random_bytes(32);
$iv  = random_bytes(openssl_cipher_iv_length('aes-256-cbc'));
$ct  = openssl_encrypt($message, 'aes-256-cbc', $key, 0, $iv);      // base64 out
$back = openssl_decrypt($ct, 'aes-256-cbc', $key, 0, $iv);          // === $message
```

It does an AES-256-CBC round-trip, an AES-256-GCM round-trip (authenticated — it also shows a
tampered tag being rejected), and a known-answer test whose output is **byte-for-byte** what
desktop OpenSSL produces.

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

## The output

From [`monitor.txt`](monitor.txt) (a real run):

```
AES-256-CBC
  decrypted matches:   yes
AES-256-GCM
  bad tag rejected:  yes
known-answer (aes-256-cbc, zero key/iv, "test"):
  d870798858223f4564d340b103f6527b
  (desktop OpenSSL: d870798858223f4564d340b103f6527b)
```

The matching known-answer is the point: ciphertext produced here decrypts on a server and
vice-versa.

## Notes

- Symmetric ciphers only. For RSA / EC / X.509 / signatures, use the [`openssl-full`](../openssl-full/)
  build.
- The key and IV come from the ESP32 hardware RNG (`random_bytes`), so they're real random.
