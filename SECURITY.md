# Security Policy

## Supported Versions

`php-esp32` is pre-1.0 and moving quickly (see [ROADMAP.md](ROADMAP.md)). Security fixes land on the
**latest release and `master`**; older tags are not back-patched. If you hit a security issue, first
check whether it still reproduces on the latest `master`.

| Version        | Supported |
|----------------|-----------|
| latest release / `master` | Yes |
| older tags     | No        |

The PHP engine itself is the unmodified upstream release from [php.net](https://www.php.net/), fetched
at build time. Vulnerabilities in PHP proper should go to the [PHP project](https://www.php.net/security),
though we're happy to bump the vendored version — the tree supports several (8.3–8.5), so a fixed
patch release usually drops in unchanged.

## Reporting a Vulnerability

> [!IMPORTANT]
> **Report to the right project.** This repository only ports and integrates PHP and ESP-IDF — it does
> not maintain them.
> - A vulnerability in the **PHP engine itself** (the language, the standard library, an `ext/*`) goes
>   to the PHP project: **[php.net/security](https://www.php.net/security)** (policy:
>   [php-src `SECURITY.md`](https://github.com/php/php-src/blob/master/SECURITY.md)). The engine here is
>   the unmodified upstream release.
> - A vulnerability in **ESP-IDF** (the SDK and the components it provides) goes to Espressif:
>   **[ESP-IDF `SECURITY.md`](https://github.com/espressif/esp-idf/blob/master/SECURITY.md)**.
>
> Report it **here** only when the issue is in *this project's own code* — the port patches, the C
> extensions, the build/config tooling, or *how* this project uses PHP or ESP-IDF.

**Do not open a public issue for a security vulnerability.**

Report it privately, either way:

- **Email** [security@php-baremetal.com](mailto:security@php-baremetal.com), or
- Use **GitHub's private vulnerability reporting**: on the
  [`php-baremetal/php-esp32`](https://github.com/php-baremetal/php-esp32) repository, go to the
  **Security** tab → **Report a vulnerability** (opens a private advisory visible only to the maintainers).

Please include:

- A description of the vulnerability
- Steps to reproduce (a minimal project / `index.php` and the board involved, if relevant)
- The affected version(s) — the php-esp32 release, the PHP version built, and the board profile
- Any potential impact assessment

You should receive an acknowledgment within a few days. Because this is a spare-time project, please
allow reasonable time for a fix before any public disclosure — we'll keep you in the loop and credit
you if you'd like.

## Scope

This policy covers the firmware in this repository. Vulnerabilities in the related tooling belong to
their own repositories, but you can report them the same private way:

- [flash-tool](https://github.com/php-baremetal/flash-tool) — the `phpflash` CLI
- the project website

Out of scope, and to be reported to their own maintainers (see the note above), not here:

- the unmodified **PHP engine** → [php.net/security](https://www.php.net/security) /
  [php-src `SECURITY.md`](https://github.com/php/php-src/blob/master/SECURITY.md)
- **ESP-IDF** → [ESP-IDF `SECURITY.md`](https://github.com/espressif/esp-idf/blob/master/SECURITY.md)
- other third-party components the build pulls in → their respective maintainers

…unless the issue is in *how this project uses* them.

## Security Considerations

Running real PHP on a microcontroller has a threat model that's different from a PHP app on a server.
A few things are worth understanding before you deploy a board somewhere it can be reached.

### There is no OS isolation

This is bare metal on FreeRTOS: **there is no process separation, no memory protection, and no
privilege boundary.** PHP code runs with full access to the whole device — GPIO, flash, network, the C
side. A script you run is as trusted as the firmware. Only run PHP you wrote or trust, and treat the
`index.php` on the board as privileged code, not a sandbox.

### Network services have no built-in access control

When a project exposes a network service (for example the web-server model, or anything your PHP opens
a socket for), that service has no authentication, transport security, or rate limiting unless your PHP
adds it. What a request can do is entirely up to your code. Don't expose a board directly to an
untrusted network or the public internet; keep it on a trusted network, or put authentication in your
PHP and a trusted network path in front.

### Secrets end up in the firmware or on the storage

Convenience features write data **into the firmware image or the board's storage**, which are not a
secret store: a project `.env`, any credentials your PHP source holds, and persistent values a script
saves all live where the code and its data live (embedded in the image, on the microSD, or in flash).

Anyone with **physical access** to the board can read the flash (and the microSD card) and recover all
of it. Flash encryption and secure boot are platform features this project does not enable for you.
Don't put long-lived or high-value secrets on a board you can't physically secure; scope credentials
narrowly and rotate them if a board is lost.

### Transport security is your responsibility

Cryptography and TLS are opt-in and only as strong as how you use them. If a board talks to something
over the network, keep the vendored PHP and the crypto/TLS stack current, use a secure transport, and
verify certificates — don't disable verification for convenience on a device that stays deployed.

### Physical and supply-chain surface

Flashing and the serial console are unauthenticated by default (that's how the hardware works). Anyone
who can physically connect to the board can reflash it or read its console. The build fetches the PHP
source and some libraries at build time; the PHP tarball is **sha256-verified** against a pinned hash,
but treat your build environment as part of the trust chain.

## Sharing Debug Logs and Reproducers

When asking for help in a **public** channel (GitHub issues, discussions, chat) or attaching a serial
log to a bug report, treat the following as sensitive and redact or omit them:

| Information                                      | Why it matters                                                                 |
|--------------------------------------------------|--------------------------------------------------------------------------------|
| Network credentials (WiFi and others)            | Boot logs and your PHP can print them; a password in a log is a live credential. |
| `.env` contents / `$_ENV` dumps                  | Whatever you baked in — API keys, tokens, endpoints.                            |
| IP addresses, hostnames, network topology        | A reachable board is a direct attack surface.                                   |
| Device identifiers (e.g. MAC addresses)          | Identify a specific device; sometimes tie back to a person or site.            |
| `phpinfo()` output                               | Reveals the build, extensions, board and paths — useful to an attacker.        |
| Any secret your script handles                   | Tokens, credentials, keys read from a sensor or a server.                      |

A serial capture of a normal boot often contains network setup and your own `echo`s — skim it before
pasting. When in doubt, **regenerate the exposed credential rather than relying on deleting the paste**;
gists, issue history and forks may already have it.

If a maintainer asks for richer diagnostics that would contain sensitive identifiers, don't paste them
into a public thread — send them to [security@php-baremetal.com](mailto:security@php-baremetal.com) (or
the private advisory) instead, referencing the issue/discussion number.
