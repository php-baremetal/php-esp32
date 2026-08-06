# Extension porting status

> The optional extensions below also have a machine-readable form, **per PHP version**, in
> `components/php/versions/<version>/manifest.toml` (for the current default,
> [8.3.32](../components/php/versions/8.3.32/manifest.toml)) — the contract the `flash-tool`
> CLI reads (flags, settings, dependencies, fetch scripts, per-type rules). The repo's default
> version is set in [`php-esp32.toml`](../php-esp32.toml); `scripts/check-manifest.py` verifies
> the manifest stays in sync with that version's build.

Every extension that ships in the PHP 8.3 source tree, and where it stands on this port.
Three states:

- **Built-in** — always compiled and registered; nothing to enable.
- **Flag** — ported but off by default; turn it on at build time with the given
  `-DPHP_EXT_*=ON` (or answer `y` in `./flash.sh`). Sizes are measured image deltas — see
  [footprint.md](footprint.md); the porting details are in [porting-notes.md](porting-notes.md).
- **Not available** — not ported. The reason is either an external library it would need
  (PHP doesn't bundle it), an OS feature this hardware/RTOS doesn't have, or simply that it's
  a self-contained extension nobody has ported yet ("portable — not yet done": a candidate,
  like `ctype`/`filter` were).

The **Roadmap** column says where each one is headed:

- **Done** — ported (every Built-in / Flag / Stub).
- **Planned** — realistic next ports (self-contained, no external library or missing OS feature),
  ranked by value-add. The top tier — the *framework enablers* `session` and `tokenizer` — is now
  **Done** (shipped, flag-gated below); the rest, in order:
  - **Planned #2** — self-contained *utilities* with broad value: `bcmath` (arbitrary-precision
    math) and `phar` (bundling an app into a single archive).
  - **Planned #3** — *niche or situational*: `exif` (image metadata — handy with the P4's camera).
- **Not planned** — could be ported (usually by bundling an external library, or just the work),
  but nobody's committed to it. Ask if you need one and it can move up.
- **Not possible** — relies on something this bare-metal target fundamentally lacks (`fork`,
  processes, shared memory, a dynamic loader, an interactive TTY, Windows COM).

## The always-on core

The language itself (the Zend engine) plus these extensions are compiled unconditionally:

`Core` · `standard` · `pcre` · `hash` · `json` · `spl` · `reflection` · `random`

Plus one non-standard extension of ours: **`gpio`** (`gpio_mode`/`gpio_write`/`gpio_read`/
`delay` and the `GPIO_*` constants) — see [porting-notes.md](porting-notes.md).

## Full list

| Extension | Status | Roadmap | Flag / reason |
|---|---|---|---|
| `bcmath` | Not available | Planned #2 | portable (pure C, arbitrary precision) — planned (self-contained math utility) |
| `bz2` | Not available | Not planned | needs libbz2 |
| `calendar` | Not available | Not planned | portable (pure C), but calendar-conversion is too niche to prioritize now |
| `com_dotnet` | Not available | Not possible | Windows-only (COM) |
| `ctype` | **Flag** | Done | `-DPHP_EXT_CTYPE=ON` (~2.5 KB) |
| `curl` | Not available | Not planned | needs libcurl (not bundled). Networking + TLS now exist (esp-tls), but libcurl itself isn't ported — for HTTPS use `openssl` `full`+`tls` and `https://` streams instead |
| `date` | **Stub + Flag** | Done | a UTC stub is always present; full `DateTime` with `-DPHP_EXT_DATE=ON` (~650 KB, or ~300 KB with `-DPHP_EXT_DATE_MINIMAL_TZ=ON`) |
| `dba` | Not available | Not planned | needs a dbm backend library |
| `dom` | Not available | Not planned | needs libxml2 |
| `enchant` | Not available | Not planned | needs enchant/aspell |
| `exif` | Not available | Planned #3 | portable (pure C) — planned (niche: image metadata, handy with the P4 camera) |
| `ffi` | Not available | Not possible | needs libffi + runtime dynamic calls (no dynamic loader here) |
| `fileinfo` | Not available | Not planned | portable but carries the large libmagic database — not done |
| `filter` | **Flag** | Done | `-DPHP_EXT_FILTER=ON` (~27 KB) |
| `ftp` | Not available | Not planned | networking exists now (esp-tls board); `ext/ftp` not ported |
| `gd` | Not available | Not planned | needs libpng/libjpeg/freetype |
| `gettext` | Not available | Not planned | needs libintl |
| `gmp` | Not available | Not planned | needs libgmp |
| `hash` | **Built-in** | Done | always on |
| `iconv` | Not available | Not planned | needs libiconv (newlib has no full iconv) |
| `imap` | Not available | Not planned | needs the c-client library |
| `intl` | Not available | Not planned | needs ICU (very large) |
| `json` | **Built-in** | Done | always on |
| `ldap` | Not available | Not planned | needs OpenLDAP |
| `libxml` | Not available | Not planned | needs libxml2 |
| `mbstring` | **Flag** | Done | `-DPHP_EXT_MBSTRING=ON` (~965 KB; `-DPHP_EXT_MBSTRING_NO_CJK=ON` → ~209 KB; `-DPHP_EXT_MBSTRING_ONIG=ON` adds `mb_ereg*`/`mb_split`, ~445 KB) |
| `mysqli` | Not available | Not planned | needs mysqlnd; networking exists now — not ported |
| `mysqlnd` | Not available | Not planned | database client; networking exists now — not ported |
| `oci8` | Not available | Not planned | needs the Oracle client |
| `odbc` | Not available | Not planned | needs unixODBC |
| `opcache` | **Flag** | Done | `-DPHP_EXT_OPCACHE=ON` — bytecode cache, no JIT. File-cache on the microSD by default; in-RAM (PSRAM) with `in_memory`. Laravel ~12 s → ~8.4 s/request. See [opcache.md](opcache.md) |
| `openssl` | **Flag** | Done | `-DPHP_EXT_OPENSSL=ON` (mbedTLS-backed subset — symmetric AES, ~42 KB); `-DPHP_EXT_OPENSSL_FULL=ON` builds the real OpenSSL 3.0 (RSA/EC/X.509/digests + on-chip keygen, ~2.1 MB); add `-DPHP_EXT_OPENSSL_TLS=ON` for an HTTPS client (esp-tls, needs a networked board). See [openssl.md](openssl.md) |
| `pcntl` | Not available | Not possible | no processes (no `fork`) |
| `pcre` | **Built-in** | Done | always on |
| `pdo` | **Flag** | Done | `-DPHP_EXT_SQLITE=ON` (built together with `pdo_sqlite`) |
| `pdo_dblib` | Not available | Not planned | needs FreeTDS |
| `pdo_firebird` | Not available | Not planned | needs the Firebird client |
| `pdo_mysql` | Not available | Not planned | needs mysqlnd; networking exists now — not ported |
| `pdo_oci` | Not available | Not planned | needs the Oracle client |
| `pdo_odbc` | Not available | Not planned | needs unixODBC |
| `pdo_pgsql` | Not available | Not planned | needs libpq |
| `pdo_sqlite` | **Flag** | Done | `-DPHP_EXT_SQLITE=ON` (~560 KB with `pdo`; SQLite amalgamation via `scripts/fetch-sqlite.sh`) |
| `pgsql` | Not available | Not planned | needs libpq |
| `phar` | Not available | Planned #2 | portable-ish (leans on hash/spl) — planned (bundle an app into one archive) |
| `posix` | Not available | Not possible | no users/processes (the symbols it wants are weak-stubbed instead) |
| `pspell` | Not available | Not planned | needs aspell |
| `random` | **Built-in** | Done | always on |
| `readline` | Not available | Not possible | needs libreadline/editline (and there's no interactive TTY) |
| `reflection` | **Built-in** | Done | always on |
| `session` | **Flag** | Done | `-DPHP_EXT_SESSION=ON` (~50 KB): `session_start()`/`$_SESSION`, the files save handler (point `session.save_path` at a writable dir like the microSD) and user handlers |
| `shmop` | Not available | Not possible | no shared memory |
| `simplexml` | Not available | Not planned | needs libxml2 |
| `snmp` | Not available | Not planned | needs net-snmp |
| `soap` | Not available | Not planned | needs libxml2 |
| `sockets` | Not available | Not planned | lwIP provides BSD sockets now (the openssl TLS client uses them), but `ext/sockets` itself isn't ported — candidate |
| `sodium` | Not available | Not planned | needs libsodium |
| `spl` | **Built-in** | Done | always on |
| `sqlite3` | Not available | Not planned | the standalone `SQLite3` class isn't built; use `pdo_sqlite` (`-DPHP_EXT_SQLITE=ON`) instead |
| `standard` | **Built-in** | Done | always on (strings, arrays, math, url, base64, …) |
| `sysvmsg` | Not available | Not possible | no System V IPC |
| `sysvsem` | Not available | Not possible | no System V IPC |
| `sysvshm` | Not available | Not possible | no System V IPC |
| `tidy` | Not available | Not planned | needs libtidy |
| `tokenizer` | **Flag** | Done | `-DPHP_EXT_TOKENIZER=ON` (~13 KB): `token_get_all()`, `token_name()`, the `PhpToken` class and the `T_*` constants |
| `xml` | Not available | Not planned | needs libxml2 |
| `xmlreader` | Not available | Not planned | needs libxml2 |
| `xmlwriter` | Not available | Not planned | needs libxml2 |
| `xsl` | Not available | Not planned | needs libxslt |
| `zip` | Not available | Not planned | needs libzip |
| `zlib` | Not available | Not planned | needs zlib |

(Not listed: `skeleton`, `dl_test`, `zend_test` — these are the source tree's example/test
extensions, not real features.)

## Summary of the build flags

| Flag | Enables | Sub-options |
|---|---|---|
| `-DPHP_EXT_DATE=ON` | real `ext/date` (`DateTime`) | `-DPHP_EXT_DATE_MINIMAL_TZ=ON` (UTC-only tz db) |
| `-DPHP_EXT_CTYPE=ON` | `ext/ctype` | — |
| `-DPHP_EXT_FILTER=ON` | `ext/filter` | — |
| `-DPHP_EXT_MBSTRING=ON` | `ext/mbstring` | `-DPHP_EXT_MBSTRING_NO_CJK=ON`, `-DPHP_EXT_MBSTRING_ONIG=ON` |
| `-DPHP_EXT_OPENSSL=ON` | `ext/openssl` (mbedTLS subset) | `-DPHP_EXT_OPENSSL_FULL=ON` (real OpenSSL 3.0), `-DPHP_EXT_OPENSSL_NO_LOAD_CONFIG=ON`, `-DPHP_EXT_OPENSSL_TLS=ON` (HTTPS client) |
| `-DPHP_EXT_TOKENIZER=ON` | `ext/tokenizer` | — |
| `-DPHP_EXT_SESSION=ON` | `ext/session` | — |
| `-DPHP_EXT_SQLITE=ON` | `ext/pdo` + `ext/pdo_sqlite` | — |

`./flash.sh` asks about each of these interactively and fetches any extra sources on demand.

Networking itself isn't an extension — it comes with a board that has one (e.g. `esp32-p4-eth`)
and is configured in `php-esp32.config.toml` (`[network] dns`, and `[extensions.openssl]`
`tls`/`certs_path` for the HTTPS client). See [openssl.md](openssl.md) and the `https-client` example.
