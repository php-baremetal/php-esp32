# openssl-full

Public-key crypto on the microcontroller with the **full** `openssl` extension — the *real*
OpenSSL 3.0 library, cross-compiled for the chip. Unlike the mbedTLS subset (symmetric only), this
is genuine `ext/openssl`: RSA signatures, RSA encryption, the full digest list, and the rest of
the OpenSSL crypto API. It costs ~2 MB of flash.

> There are two openssl examples. This one uses the ported real OpenSSL (public-key, ~2 MB). The
> sibling [`openssl-compat`](../openssl-compat/) uses the tiny mbedTLS subset (symmetric AES only).
> See [`docs/openssl.md`](../../docs/openssl.md) for when to pick which.

## Firmware

This project's `php-esp32.config.toml` enables openssl with `full = true`. The first build runs
`scripts/fetch-openssl.sh`, which downloads OpenSSL 3.0 and cross-compiles `libcrypto` for the
chip (a few minutes, cached afterwards). `phpflash build` does this for you.

## What it does

`index.php` loads a 2048-bit RSA key (`key.pem`, shipped with the example), exercises the
public-key API, and finally **generates a fresh RSA key on the chip**:

```php
$key = openssl_pkey_get_private(file_get_contents(__DIR__ . '/key.pem'));
openssl_sign($msg, $sig, $key, OPENSSL_ALGO_SHA256);           // 256-byte RSA signature
openssl_verify($msg, $sig, $pub, OPENSSL_ALGO_SHA256);         // === 1
openssl_public_encrypt("hi chip", $enc, $pub);
openssl_private_decrypt($enc, $dec, $key);                     // === "hi chip"
echo openssl_digest('abc', 'sha3-256');                        // real EVP digests

$new = openssl_pkey_new(['private_key_bits' => 2048, 'private_key_type' => OPENSSL_KEYTYPE_RSA]);
```

The last line works because this build ships an `openssl.cnf` and the firmware points
`OPENSSL_CONF` at it (that's what OpenSSL 3.0 needs to bring up its providers — see
[`docs/openssl.md`](../../docs/openssl.md), *Configuration*). `phpflash build` writes that
`openssl.cnf` into `project-src/` for you.

## Building and running

```sh
phpflash build && phpflash flash && phpflash monitor
```

## The output

From [`monitor.txt`](monitor.txt) (a real run):

```
digests (real OpenSSL EVP -- more than the subset offers):
  sha256('abc')    = ba7816bf...20015ad
  sha3-256('abc')  = 3a985da7...11431532
  ripemd160('abc') = 8eb208f7e05d987a9b044a8e98c6b087f15a0bfc
loaded an RSA 2048-bit key
RSA-SHA256 sign/verify: valid (256-byte signature)
RSA public-encrypt / private-decrypt: OK
AES-256-CBC roundtrip: OK
generated a fresh RSA 2048-bit key on-chip in 42265 ms
sign/verify with the new key: valid
```

Real RSA and SHA-3 — including generating a key from scratch — on a 32-bit microcontroller.

## Notes

- **On-chip key generation works, but it's slow.** RSA-2048 `openssl_pkey_new` is CPU-bound and
  takes tens of seconds (~20-45 s, and it varies) hunting for primes. The base config widens the
  task-watchdog timeout so this doesn't print watchdog warnings. If you only ever *use* provisioned
  keys, you don't need it — and the leaner `no_load_config` setting skips the `openssl.cnf`
  altogether. See [`docs/openssl.md`](../../docs/openssl.md), *Configuration*.
- **EC keygen** needs curve defaults the minimal `openssl.cnf` doesn't set, and `openssl_csr_sign` /
  X.509 issuing are untested on this port — stick to RSA on-device, or provision EC keys off-device.
- The `key.pem` here is a throwaway generated for the example; use your own for anything real.
- No TLS: this is crypto only (`libcrypto`), not the `ssl://` stream transport.
