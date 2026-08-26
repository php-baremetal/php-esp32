---
eyebrow: 'Docs · Extensions'
lede: "PHP's ext/openssl on a target that ships mbedTLS, not OpenSSL. Two builds share one module and one API: a compact mbedTLS-backed subset for symmetric crypto, and the real OpenSSL 3.0 libcrypto cross-compiled for the chip, with RSA/EC, X.509, on-chip key generation, and a certificate-verified HTTPS/TLS client."
see_also:
  - href: './porting-status.md'
    meta: 'Extensions'
    label: 'Extension porting status'
  - href: '../getting-started/quick-start.md'
    meta: 'Getting started'
    label: 'Quick start'
  - href: '../reference/footprint.md'
    meta: 'Reference'
    label: 'Flash footprint'
  - href: 'https://docs.openssl.org/3.0/man7/OSSL_PROVIDER-default/'
    meta: 'OpenSSL 3.0'
    label: 'OpenSSL 3.0 default provider'
prev:
  label: 'Extension porting status'
  href: './porting-status.md'
next:
  label: 'OPcache'
  href: './opcache.md'
---

# The openssl extension: subset versus full

PHP's `ext/openssl` is written against the OpenSSL C library, which does not exist on this target.
ESP-IDF ships mbedTLS, a different API entirely, so the extension is delivered as two builds and a
project picks one at build time. Both register the same `openssl` module and expose the same
function names; they differ only in how much of OpenSSL they actually implement, and in how much
flash they cost.

<!-- @callout variant="info" title="One module, two backends" -->
Whichever build you select, PHP code calls the same `openssl_*` functions. The subset satisfies the
symmetric-crypto surface from the chip's existing mbedTLS; the full build satisfies the entire
crypto surface from a genuine OpenSSL 3.0 libcrypto cross-compiled for the microcontroller. Both are
byte-for-byte interoperable with desktop OpenSSL: an `openssl_encrypt` here decrypts on a server,
and the reverse.
<!-- @endcallout -->

## At a glance

| | Compatible subset (default) | Full |
|---|---|---|
| Build flag | `-DPHP_EXT_OPENSSL=ON` | add `-DPHP_EXT_OPENSSL_FULL=ON` |
| Backed by | ESP-IDF's mbedTLS | the real OpenSSL 3.0 libcrypto, cross-compiled for the chip |
| Flash cost | ~42 KB | ~2 MB |
| Boards | any board | ESP32-P4 today (RISC-V); Xtensa/S3 is a toolchain branch away |
| Symmetric ciphers (AES-CBC/GCM) | yes | yes |
| `openssl_random_pseudo_bytes` | yes | yes |
| `openssl_cipher_iv_length` | yes | yes |
| Public key (RSA, EC, DSA, DH) | no | yes |
| Signatures, X.509, CSR, PKCS7/PKCS12 | no | yes |
| Digests (`openssl_digest`, full algorithm list) | no | yes |
| TLS client (`https://`, `tls://`) | no | yes, with the `tls` setting (esp-tls; needs a networked board) |

The subset uses the chip's mbedTLS and builds on any board. The full build is currently produced for
the ESP32-P4 — its libcrypto is cross-compiled for RISC-V by `scripts/fetch-openssl.sh` — and an
Xtensa build for the ESP32-S3 is a matter of adding a toolchain branch to that script, not a code
change. Both flavours are verified on ESP32-P4 hardware.

## Which one to choose

<!-- @tabs labels="Subset, Full" -->
<!-- @tab index="0" -->

Use the subset if you only need symmetric encryption: `openssl_encrypt` and `openssl_decrypt` with
AES-CBC or AES-GCM, plus `openssl_cipher_iv_length`. That covers the common "encrypt a blob with a
shared key" case, including a framework's encrypter — Laravel's `Encrypter` uses AES-256-CBC/GCM. It
is tiny (~42 KB) and pulls in nothing new at runtime, since mbedTLS is already on the chip. This is
the default, so `-DPHP_EXT_OPENSSL=ON` on its own gives it to you.

<!-- @endtab -->
<!-- @tab index="1" -->

Use the full build if you need public-key crypto or certificates: RSA and EC keys, `openssl_sign`
and `openssl_verify`, the `openssl_pkey_*` family, X.509 (`openssl_x509_*`, `openssl_csr_*`),
PKCS7/PKCS12, and the full `openssl_digest` algorithm list. It is the genuine `ext/openssl` on a
genuine OpenSSL, so the API matches a normal PHP host. The cost is about 2 MB of flash and a
one-time source cross-build.

<!-- @endtab -->
<!-- @endtabs -->

If you are not sure, start with the subset. Switching to full later is one flag.

## Building

phpflash derives the CMake flags from the project config. The `[extensions.openssl]` table controls
the whole thing:

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
[extensions.openssl]
enabled        = true
full           = false   # true = the real OpenSSL (full API, ~2 MB); false = mbedTLS subset
no_load_config = false   # full only: true = do not read openssl.cnf (see Configuration)
```
<!-- @endcode-block -->

By hand, the mapping is direct:

| Config key | CMake flag | Effect |
|---|---|---|
| `enabled = true` | `-DPHP_EXT_OPENSSL=ON` | build the extension (subset by default) |
| `full = true` | add `-DPHP_EXT_OPENSSL_FULL=ON` | swap in the real OpenSSL 3.0 libcrypto |
| `no_load_config = true` | `-DPHP_EXT_OPENSSL_NO_LOAD_CONFIG=ON` | full build skips `openssl.cnf` at startup |
| `config_path = "…"` | `-DPHP_OPENSSL_CONF=…` | full build reads the cnf from a non-default path |
| `tls = true` | builds the `ssl://` / `tls://` transport | full build gains an HTTPS/TLS client |
| `[network] dns = […]` | `-DPHP_NET_DNS=…` | static DNS servers, comma-joined for the firmware |

The full build needs the OpenSSL source cross-compiled first. `phpflash build` runs it for you; by
hand:

<!-- @code-block language="sh" label="Cross-compile libcrypto for the chip" -->
```sh
. $IDF_PATH/export.sh
./scripts/fetch-openssl.sh     # downloads OpenSSL 3.0.15 and builds libcrypto.a for the chip (slow)
```
<!-- @endcode-block -->

That produces `components/php/openssl-build/{lib/libcrypto.a,include/}` (git-ignored), referenced by
`versions/<ver>/openssl-full.cmake`. It takes a few minutes the first time and is cached after that
— the script is idempotent and exits early if `libcrypto.a` already exists.

<!-- @callout variant="warning" title="Flash cost" -->
The full build adds roughly 2 MB of flash for the real libcrypto. The subset is ~42 KB because it
reuses the mbedTLS already linked into the firmware. Budget the 2 MB before you switch; see the
[flash footprint](../reference/footprint.md) reference.
<!-- @endcallout -->

## How the full port works

The full build compiles the actual `ext/openssl/openssl.c` against a ported OpenSSL 3.0 libcrypto.
`scripts/fetch-openssl.sh` cross-compiles OpenSSL for the chip's toolchain plus newlib — statically,
without PIC (bare metal has no dynamic loader, so a `.got.plt` would fail to link), with a set of
`no-*` options (no asm, threads, sockets, DSO, engines, or legacy provider) and a one-line
`<syslog.h>` shim newlib lacks.

<!-- @code-block language="sh" label="scripts/fetch-openssl.sh — the Configure line" -->
```sh
./Configure linux-generic32 \
    no-asm no-shared no-pic no-threads no-dso no-engine no-tests no-ui-console no-sock \
    no-dgram no-module no-legacy no-secure-memory no-afalgeng no-comp \
    --cross-compile-prefix=riscv32-esp-elf- --with-rand-seed=getrandom \
    -march=rv32imafc_zicsr_zifencei -mabi=ilp32f "-I${SHIM}" -w
```
<!-- @endcode-block -->

The one runtime adaptation is entropy. There is no `/dev/urandom`, so the DRBG is seeded from
`getrandom()`, which newlib provides backed by the ESP32 hardware RNG (`esp_fill_random`); the
firmware additionally installs a legacy `RAND_METHOD` on the same hardware source, so key, IV and
nonce generation work.

The crypto stays real OpenSSL; the TLS transport rides mbedTLS. The full build compiles `openssl.c`
— all the crypto functions — but not OpenSSL's own `xp_ssl.c` TLS stream, which would need `libssl`
plus BSD sockets, and the standalone cross-build has neither (hence `no-sock`). So the `ssl://` and
`tls://` stream transport is provided separately, backed by ESP-IDF's esp-tls and mbedTLS, and only
when you ask for it with the `tls` setting.

## TLS client (HTTPS): the `tls` setting

By default the full build is crypto only: the `ssl://` and `tls://` transports are registered but
refuse to open, so `https://` is unavailable. Turn on the `tls` setting to build a working TLS
client:

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
[extensions.openssl]
enabled = true
full    = true
tls     = true      # build the ssl:// and tls:// transport, so https:// works

[network]
dns = ["1.1.1.1", "8.8.8.8"]   # optional static DNS (DHCP-provided otherwise)
```
<!-- @endcode-block -->

Then, from PHP on the chip:

<!-- @code-block language="php" label="index.php — HTTPS from the microcontroller" -->
```php
$html = file_get_contents('https://example.com/');          // verified HTTPS GET
$fp   = stream_socket_client('tls://api.example.com:443');  // raw TLS socket
```
<!-- @endcode-block -->

<!-- @callout variant="warning" title="Requirements for the TLS client" -->
The `tls` setting needs a networked board, a CA bundle on the device, and software AES. Read the
four points below before you rely on it.
<!-- @endcallout -->

- **A networked board.** The transport does DNS, TCP and TLS in one esp-tls call, so name resolution
  rides the board's network (DHCP DNS, or the static `[network] dns`, comma-joined into
  `-DPHP_NET_DNS`).
- **Root certificates.** OpenSSL 3.0's TLS still verifies peers, so the device needs a CA bundle.
  `phpflash build` copies your host's root-CA store into the project (`project-src/certs/ca-bundle.crt`
  by default) so it ships to the device, and the firmware points the transport at it. Without a
  bundle the transport still connects but does not verify the peer (it logs a warning) — fine for a
  quick test, not for production.
- **Software AES.** The base config disables mbedTLS's hardware AES. The PHP heap is PSRAM, and the
  AES accelerator's DMA cannot reach PSRAM, so the handshake would fail to allocate DMA descriptors.
  Software AES works from any memory. A handshake takes around 10 s (software RSA cert-chain verify
  plus AES), fine for occasional requests, and the watchdog window covers it.
- **Client only.** There is no TLS server (accept). Because this is the crypto library's transport
  rather than OpenSSL's own `libssl`, a few OpenSSL-specific stream-context options
  (`peer_fingerprint`, `capture_peer_cert`) are not honored. The common client cases — verified
  `https://` and `tls://` sockets — work.

Verified on ESP32-P4-ETH hardware: `file_get_contents('https://example.com/')` returns the page over
a certificate-verified TLS 1.2/1.3 connection.

### Static DNS

The esp-tls transport resolves names over the board's network. With DHCP the resolver's DNS servers
are used as provided. To pin your own resolvers, set `[network] dns`; phpflash passes them to the
firmware as `-DPHP_NET_DNS=1.1.1.1,8.8.8.8` (comma-separated). Leave the array empty to keep the
DHCP-provided DNS.

<!-- @code-block language="toml" label="Static DNS servers" -->
```toml
[network]
dns = ["1.1.1.1", "8.8.8.8"]   # empty or omitted = DHCP-provided DNS
```
<!-- @endcode-block -->

### Certificates and the CA bundle

The bundle ships at `project-src/certs/ca-bundle.crt` (the firmware default `certs/ca-bundle.crt`,
resolved against the source folder), overridable with `certs_path` — a relative path is where
phpflash writes it and where the firmware reads it; an absolute path is an on-device path you manage
yourself. By default phpflash copies the first system trust store it finds, in order:

| Order | Host bundle | Typical distro |
|---|---|---|
| 1 | `/etc/pki/tls/certs/ca-bundle.crt` | Fedora, RHEL |
| 2 | `/etc/ssl/certs/ca-certificates.crt` | Debian, Ubuntu |
| 3 | `/etc/ssl/certs/ca-bundle.crt` | others |

Set `certs_source` to force a specific host file. The shipped bundle is git-ignored in the examples,
being a host-specific, regenerated artifact.

<!-- @code-block language="toml" label="Explicit certificate paths" -->
```toml
[extensions.openssl]
enabled      = true
full         = true
tls          = true
certs_path   = "certs/ca-bundle.crt"                 # where it ships (default)
certs_source = "/etc/pki/tls/certs/ca-bundle.crt"    # host bundle to copy (auto-detected if unset)
```
<!-- @endcode-block -->

`phpflash build` writes the bundle once and never overwrites it. To refresh it later — renewed roots,
or a new `certs_source` — run `phpflash update-certs`, which re-copies the host trust store into
`certs_path`, overwriting the current bundle.

<!-- @code-block language="sh" label="Refresh the CA bundle" -->
```sh
phpflash update-certs        # updated CA bundle: /etc/pki/tls/certs/ca-bundle.crt -> certs/ca-bundle.crt
```
<!-- @endcode-block -->

<!-- @callout variant="info" title="Why the refresh is a separate command" -->
`phpflash build` ships the bundle on the first build and then leaves it alone, so a rebuild never
silently changes what your device trusts. `update-certs` is the deliberate way to pull in new or
renewed root CAs from the host.
<!-- @endcallout -->

## Configuration: openssl.cnf (full build)

OpenSSL 3.0 does its real work through providers — the `default` provider holds RSA, EC, the digests
and the DRBG. On a normal host it finds and activates them by reading an `openssl.cnf` at startup.
There is no such file on a bare-metal chip by default, and without it some provider-backed
operations — most notably generating a new key (`openssl_pkey_new`) and issuing certificates
(`openssl_csr_sign`) — fail deep inside OpenSSL with a "configuration file routines: no such file"
error. So the full build makes you decide how it initializes. There are two supported modes; pick
one in the project config.

<!-- @tabs labels="Mode 1 — ship a cnf, Mode 2 — skip the cnf" -->
<!-- @tab index="0" -->

Default and recommended. Leave `no_load_config` off (its default is `false`).

<!-- @code-block language="toml" label="Mode 1 — read openssl.cnf at startup" -->
```toml
[extensions.openssl]
enabled = true
full    = true
# no_load_config omitted, so false: read openssl.cnf at startup
```
<!-- @endcode-block -->

`phpflash build` writes a minimal `openssl.cnf` into your `project-src/` (next to `index.php`) if you
do not already have one, and the firmware sets `OPENSSL_CONF` to that path so OpenSSL reads it and
brings the default provider up the normal way. The shipped file is just enough to activate the
default provider:

<!-- @code-block language="ini" label="openssl.cnf (generated)" -->
```ini
openssl_conf = openssl_init
[openssl_init]
providers = provider_sect
[provider_sect]
default = default_sect
[default_sect]
activate = 1
```
<!-- @endcode-block -->

Use this when you want the full OpenSSL surface, including on-chip key generation. Verified on
hardware: with the `openssl.cnf` in place, `openssl_pkey_new(['private_key_bits' => 2048])` generates
a real RSA key on the chip and signs and verifies with it. You can edit the shipped file — for
instance to also activate the `legacy` provider — and phpflash never overwrites one that already
exists.

By default the file is `openssl.cnf` in the source folder. To put it elsewhere, set `config_path`: a
relative path is where phpflash writes the generated file and where the firmware reads it (resolved
against the source mount, passed as `-DPHP_OPENSSL_CONF`); an absolute path is used verbatim as the
on-device path, and phpflash does not create it, so you ship that file yourself.

<!-- @endtab -->
<!-- @tab index="1" -->

Leaner. Set `no_load_config = true` and ship no config file.

<!-- @code-block language="toml" label="Mode 2 — never load openssl.cnf" -->
```toml
[extensions.openssl]
enabled        = true
full           = true
no_load_config = true   # firmware never loads openssl.cnf; ship no config file
```
<!-- @endcode-block -->

This builds with `-DPHP_EXT_OPENSSL_NO_LOAD_CONFIG=ON`; the firmware calls
`OPENSSL_init_crypto(OPENSSL_INIT_NO_LOAD_CONFIG)` and never touches a config file. The default
provider still loads implicitly, so digests, symmetric ciphers, and operations on a pre-existing key
(sign and verify, public-encrypt and private-decrypt, `openssl_pkey_get_*`) all work. What you give
up is the config-driven init that on-chip key generation and certificate issuing rely on.

Use this when your device only ever uses keys shipped with it — the common embedded case: a
provisioned device key, signing, verifying and decrypting with it, and you would rather not carry an
`openssl.cnf` in the image. It is the smallest, simplest setup when you do not need to mint new keys
on the chip.

<!-- @endtab -->
<!-- @endtabs -->

<!-- @callout variant="info" title="Rule of thumb" -->
Generate keys on the chip → Mode 1. Only ever use keys you provisioned off-device → Mode 2 is fine
and leaner.
<!-- @endcallout -->

## On-chip key generation

RSA key generation with `openssl_pkey_new` works in Mode 1 and is verified on hardware. It is
CPU-bound: RSA-2048 takes tens of seconds on the chip — the primality search measured around 20 to
45 s across runs, and being probabilistic it varies. The base config raises the task-watchdog
timeout to 60 s so that the long compute does not print warnings; it never panics regardless. In
Mode 2 key generation fails in OpenSSL's provider init, which is the one thing the config unlocks.

<!-- @code-block language="php" label="Generate an RSA key on the chip (Mode 1)" -->
```php
$new = openssl_pkey_new([
    'private_key_bits' => 2048,
    'private_key_type' => OPENSSL_KEYTYPE_RSA,
]);   // ~20-45 s of prime hunting on the chip
```
<!-- @endcode-block -->

<!-- @callout variant="warning" title="EC keygen and X.509 issuing" -->
EC key generation via `openssl_pkey_new` needs curve defaults the minimal `openssl.cnf` does not set
(you will see "Private key length must be at least 384 bits, configured to 0"). Stick to RSA
on-device, or ship EC keys provisioned off-device. `openssl_csr_sign` and X.509 issuing are likewise
untested on this port.
<!-- @endcallout -->

## Worked example: the openssl-full example

The [`openssl-full`](https://github.com/php-baremetal/php-esp32/tree/master/examples/openssl-full) example loads a shipped 2048-bit RSA key, exercises
the public-key API, and finally generates a fresh RSA key on the chip. Its config uses microSD
storage and enables the full build:

<!-- @code-block language="php" label="index.php — the public-key path" -->
```php
$key = openssl_pkey_get_private(file_get_contents(__DIR__ . '/key.pem'));
openssl_sign($msg, $sig, $key, OPENSSL_ALGO_SHA256);           // 256-byte RSA signature
openssl_verify($msg, $sig, $pub, OPENSSL_ALGO_SHA256);         // === 1
openssl_public_encrypt("hi chip", $enc, $pub);
openssl_private_decrypt($enc, $dec, $key);                     // === "hi chip"
echo openssl_digest('abc', 'sha3-256');                        // real EVP digests

$new = openssl_pkey_new(['private_key_bits' => 2048, 'private_key_type' => OPENSSL_KEYTYPE_RSA]);
```
<!-- @endcode-block -->

A real run on ESP32-P4 hardware (from the example's `monitor.txt`):

<!-- @code-block language="text" label="monitor.txt — a real run" -->
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
<!-- @endcode-block -->

## Symmetric crypto (both builds)

The symmetric surface is identical across the two builds and is the whole of the subset. Both use the
hardware RNG for entropy, so keys, IVs and nonces are real random.

<!-- @code-block language="php" label="AES round-trip — works on subset and full" -->
```php
$key = openssl_random_pseudo_bytes(32);                        // AES-256 key from the hardware RNG
$iv  = openssl_random_pseudo_bytes(openssl_cipher_iv_length('aes-256-cbc'));
$ct  = openssl_encrypt($plaintext, 'aes-256-cbc', $key, OPENSSL_RAW_DATA, $iv);
$pt  = openssl_decrypt($ct, 'aes-256-cbc', $key, OPENSSL_RAW_DATA, $iv);
```
<!-- @endcode-block -->

The ciphertext is interoperable with desktop OpenSSL in both directions.

## Notes (full build)

- **Always works, either mode.** Digests (`openssl_digest`, the full list: SHA-2, SHA-3, RIPEMD),
  RSA and EC operations on an existing key (`openssl_sign`, `openssl_verify`,
  `openssl_public_encrypt`, `openssl_private_decrypt`, `openssl_pkey_get_*`) and the symmetric
  ciphers. All verified on hardware; see the [`openssl-full`](https://github.com/php-baremetal/php-esp32/tree/master/examples/openssl-full) example.
- **On-chip RSA key generation** (`openssl_pkey_new`) works in Mode 1, verified on hardware. It is
  CPU-bound (~20 to 45 s for RSA-2048); the widened watchdog covers it. In Mode 2 it fails in
  OpenSSL's provider init.
- **EC key generation** needs curve defaults the minimal `openssl.cnf` does not set. Stick to RSA
  on-device, or ship EC keys provisioned off-device. `openssl_csr_sign` and X.509 issuing are
  untested on this port.
- **HTTPS and TLS client** is available with the `tls` setting on a networked board (see above); DNS
  resolution and certificate-verified `https://` both work. The subset and a plain full build
  without `tls` have no TLS transport.
- **Random bytes** use the hardware RNG in both flavours (`random_bytes`,
  `openssl_random_pseudo_bytes`), so symmetric keys, IVs and nonces have real entropy.
