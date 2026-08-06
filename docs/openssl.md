# The openssl extension: subset versus full

PHP's `ext/openssl` is written against the OpenSSL C library, which does not exist for this target
(ESP-IDF ships mbedTLS, a different API). So the extension comes in two builds, and a project picks
one. Both register the same `openssl` module and the same function names; they differ in how much of
OpenSSL they actually implement, and in size.

| | Compatible subset (default) | Full |
|---|---|---|
| Build | `-DPHP_EXT_OPENSSL=ON` | add `-DPHP_EXT_OPENSSL_FULL=ON` |
| Backed by | ESP-IDF's mbedTLS | the real OpenSSL 3.0 libcrypto, cross-compiled for the chip |
| Flash cost | ~42 KB | ~2 MB |
| Symmetric ciphers (AES-CBC/GCM) | yes | yes |
| `openssl_random_pseudo_bytes` | yes | yes |
| Public key (RSA, EC, DSA, DH) | no | yes |
| Signatures, X.509, CSR, PKCS7/12 | no | yes |
| Digests (`openssl_digest`, many algorithms) | no | yes |
| TLS client (`https://`, `tls://`) | no | yes, with the `tls` setting (esp-tls; needs a networked board) |

The subset uses the chip's mbedTLS and builds on any board. The full build is currently produced for
the ESP32-P4 (its libcrypto is cross-compiled for RISC-V by `scripts/fetch-openssl.sh`); an Xtensa
build for the ESP32-S3 is a matter of adding a toolchain branch to that script, not a code change.
Both flavours are verified on ESP32-P4 hardware, and both are byte-for-byte interoperable with desktop
OpenSSL: an `openssl_encrypt` here decrypts on a server, and the reverse.

## Which one to choose

Use the subset if you only need symmetric encryption: `openssl_encrypt` and `openssl_decrypt` with
AES-CBC or AES-GCM, plus `openssl_cipher_iv_length`. That covers the common "encrypt a blob with a
shared key" case, including a framework's encrypter (Laravel's `Encrypter` uses AES-256-CBC/GCM). It
is tiny and pulls in nothing new at runtime, since mbedTLS is already on the chip. This is the
default, so `-DPHP_EXT_OPENSSL=ON` on its own gives it to you.

Use the full build if you need public-key crypto or certificates: RSA and EC keys, `openssl_sign` and
`openssl_verify`, the `openssl_pkey_*` family, X.509 (`openssl_x509_*`, `openssl_csr_*`), PKCS7/PKCS12,
and the full `openssl_digest` algorithm list. It is the genuine `ext/openssl` on a genuine OpenSSL, so
the API matches a normal PHP host. The cost is about 2 MB of flash and a one-time source build.

If you are not sure, start with the subset. Switching to full later is one flag.

## Building

phpflash sets the flags from the project config:

```toml
[extensions.openssl]
enabled        = true
full           = false   # true = the real OpenSSL (full API, ~2 MB); false = mbedTLS subset
no_load_config = false   # full only: true = do not read openssl.cnf (see Configuration)
```

By hand, `-DPHP_EXT_OPENSSL=ON` for the subset, add `-DPHP_EXT_OPENSSL_FULL=ON` for full, and
`-DPHP_EXT_OPENSSL_NO_LOAD_CONFIG=ON` to skip the config file.

The full build needs the OpenSSL source cross-compiled first. `phpflash build` runs it for you; by
hand:

```sh
. $IDF_PATH/export.sh
./scripts/fetch-openssl.sh     # downloads OpenSSL 3.0.15 and builds libcrypto.a for the chip (slow)
```

That produces `components/php/openssl-build/{lib/libcrypto.a,include/}` (git-ignored). It takes a few
minutes the first time and is cached after that.

## How the full port works

The full build is the actual `ext/openssl/openssl.c` compiled against a ported OpenSSL 3.0 libcrypto.
`fetch-openssl.sh` cross-compiles OpenSSL for the chip's toolchain plus newlib, statically and without
PIC (bare metal has no dynamic loader, so a `.got.plt` would fail to link), with a few `no-*` options
(no asm, threads, sockets or engines) and a one-line `<syslog.h>` shim newlib lacks. The only runtime
adaptation is entropy: OpenSSL is configured with `--with-rand-seed=none`, since there is no
`/dev/urandom`, and the firmware installs a `RAND_METHOD` backed by the ESP32 hardware RNG
(`esp_fill_random`) at startup, so key, IV and nonce generation work.

The crypto stays real OpenSSL; the TLS transport rides mbedTLS. The full build compiles `openssl.c`
(all the crypto functions) but not OpenSSL's own `xp_ssl.c` TLS stream, which would need `libssl` plus
BSD sockets, and the standalone cross-build has neither (hence `no-sock`). So the `ssl://` and `tls://`
stream transport is provided separately, backed by ESP-IDF's esp-tls and mbedTLS, and only when you
ask for it (the `tls` setting).

## TLS client (HTTPS): the `tls` setting

By default the full build is crypto only: the `ssl://` and `tls://` transports are registered but
refuse to open, so `https://` is unavailable. Turn on the `tls` setting to build a working TLS
client:

```toml
[extensions.openssl]
enabled = true
full    = true
tls     = true      # build the ssl:// and tls:// transport, so https:// works

[network]
dns = ["1.1.1.1", "8.8.8.8"]   # optional static DNS (DHCP-provided otherwise)
```

Then, from PHP on the chip:

```php
$html = file_get_contents('https://example.com/');          // verified HTTPS GET
$fp   = stream_socket_client('tls://api.example.com:443');  // raw TLS socket
```

Requirements and behavior:

- **A networked board.** The transport does DNS, TCP and TLS in one esp-tls call, so name resolution
  rides the board's network (DHCP DNS, or the static `[network] dns`).
- **Root certificates.** OpenSSL 3.0's TLS still verifies peers, so the device needs a CA bundle.
  `phpflash build` copies your host's root-CA store into the project (`project-src/certs/ca-bundle.crt`
  by default) so it ships to the device, and the firmware points the transport at it. Without a bundle
  the transport still connects but does not verify the peer (it logs a warning), which is fine for a
  quick test and not for production.
- **Software AES.** The base config disables mbedTLS's hardware AES. The PHP heap is PSRAM, and the
  AES accelerator's DMA cannot reach PSRAM, so the handshake would fail to allocate DMA descriptors.
  Software AES works from any memory. A handshake takes around 10 s (software RSA cert-chain verify
  plus AES), fine for occasional requests, and the watchdog window covers it.
- **Client only.** There is no TLS server (accept), and because this is the crypto library's transport
  rather than OpenSSL's own `libssl`, a few OpenSSL-specific stream-context options
  (`peer_fingerprint`, `capture_peer_cert`) are not honored. The common client cases, verified
  `https://` and `tls://` sockets, work.

Verified on ESP32-P4-ETH hardware: `file_get_contents('https://example.com/')` returns the page over a
certificate-verified TLS 1.2/1.3 connection.

### Certificates

The bundle location is `project-src/certs/ca-bundle.crt`, overridable with `certs_path` (relative to
the source folder, or an absolute on-device path you manage yourself). By default phpflash copies the
first system trust store it finds (`/etc/pki/tls/certs/ca-bundle.crt` on Fedora and RHEL,
`/etc/ssl/certs/ca-certificates.crt` on Debian and Ubuntu); set `certs_source` to force a specific
file. It is git-ignored in the examples, being a host-specific, regenerated artifact.

`phpflash build` writes the bundle once and never overwrites it. To refresh it later (renewed roots, a
new `certs_source`), run `phpflash update-certs`, which re-copies the host trust store into
`certs_path`.

```toml
[extensions.openssl]
enabled      = true
full         = true
tls          = true
certs_path   = "certs/ca-bundle.crt"                 # where it ships (default)
certs_source = "/etc/pki/tls/certs/ca-bundle.crt"    # host bundle to copy (auto-detected if unset)
```

## Configuration: openssl.cnf (full build)

OpenSSL 3.0 does its real work through providers (the `default` provider holds RSA, EC, the digests
and the DRBG). On a normal host it finds and activates them by reading an `openssl.cnf` at startup.
There is no such file on a bare-metal chip by default, and without it some provider-backed operations,
most notably generating a new key (`openssl_pkey_new`) and issuing certificates (`openssl_csr_sign`),
fail deep inside OpenSSL with a "configuration file routines: no such file" error. So the full build
needs you to decide how it initializes. There are two supported modes; pick one in the project config.

### Mode 1 (default, recommended): ship an openssl.cnf

```toml
[extensions.openssl]
enabled = true
full    = true
# no_load_config omitted, so false: read openssl.cnf at startup
```

`phpflash build` writes a minimal `openssl.cnf` into your `project-src/` (next to `index.php`) if you
do not already have one, and the firmware sets `OPENSSL_CONF` to that path so OpenSSL reads it and
brings the default provider up the normal way. The file it ships is just enough to activate the
default provider:

```ini
openssl_conf = openssl_init
[openssl_init]
providers = provider_sect
[provider_sect]
default = default_sect
[default_sect]
activate = 1
```

Use this when you want the full OpenSSL surface, including on-chip key generation. Verified on
hardware: with the `openssl.cnf` in place, `openssl_pkey_new(['private_key_bits' => 2048])` generates
a real RSA key on the chip and signs and verifies with it; the [`openssl-full`](../examples/openssl-full/)
example does exactly this at boot. You can edit the shipped `openssl.cnf` (for instance to also
activate the `legacy` provider); phpflash never overwrites one that already exists.

By default the file is `openssl.cnf` in the source folder. To put it elsewhere, set `config_path`. A
relative path is where phpflash writes the generated file and where the firmware reads it (resolved
against the source mount); an absolute path is used verbatim as the on-device path, and phpflash does
not create it, so you ship that file yourself.

### Mode 2: skip the config (`no_load_config = true`)

```toml
[extensions.openssl]
enabled        = true
full           = true
no_load_config = true   # firmware never loads openssl.cnf; ship no config file
```

This builds with `-DPHP_EXT_OPENSSL_NO_LOAD_CONFIG=ON`; the firmware calls
`OPENSSL_init_crypto(OPENSSL_INIT_NO_LOAD_CONFIG)` and never touches a config file. The default
provider still loads implicitly, so digests, symmetric ciphers, and operations on a pre-existing key
(sign and verify, public-encrypt and private-decrypt, `openssl_pkey_get_*`) all work. What you give up
is the config-driven init that on-chip key generation and certificate issuing rely on.

Use this when your device only ever uses keys shipped with it, which is the common embedded case: a
provisioned device key, signing, verifying and decrypting with it, and you would rather not carry an
`openssl.cnf` in the image. It is the smallest, simplest setup when you do not need to mint new keys on
the chip. As a rule of thumb: generate keys on the chip means Mode 1; only use keys you provisioned
means Mode 2 is fine and leaner.

## Notes (full build)

- **Always works, either mode.** Digests (`openssl_digest`, the full list: SHA-2, SHA-3, RIPEMD), RSA
  and EC operations on an existing key (`openssl_sign`, `openssl_verify`, `openssl_public_encrypt`,
  `openssl_private_decrypt`, `openssl_pkey_get_*`) and the symmetric ciphers. All verified on hardware;
  see the [`openssl-full`](../examples/openssl-full/) example.
- **On-chip RSA key generation** (`openssl_pkey_new`) works in Mode 1, verified on hardware. It is
  CPU-bound: RSA-2048 takes tens of seconds on the chip (the primality search measured around 20 to 45
  s across runs, and being probabilistic it varies). The base config raises the task-watchdog timeout
  to 60 s so that long compute does not print warnings; it never panics regardless. In Mode 2 key
  generation fails in OpenSSL's provider init, which is the one thing the config unlocks.
- **EC key generation** via `openssl_pkey_new` needs curve defaults the minimal `openssl.cnf` does not
  set (you will see "Private key length must be at least 384 bits, configured to 0"). Stick to RSA
  on-device, or ship EC keys provisioned off-device. `openssl_csr_sign` and X.509 issuing are likewise
  untested on this port.
- **HTTPS and TLS client** is available with the `tls` setting on a networked board (see above); DNS
  resolution and certificate-verified `https://` both work. The subset and a plain full build without
  `tls` have no TLS transport.
- **Random bytes** use the hardware RNG in both flavours (`random_bytes`,
  `openssl_random_pseudo_bytes`), so symmetric keys, IVs and nonces have real entropy.
