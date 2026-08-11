# Extension porting status

Every extension that ships in the PHP source tree, and where it stands on this port. There is a
machine-readable version of the optional extensions, per PHP version, in
`components/php/versions/<version>/manifest.toml` (for the current default,
[8.4.24](../components/php/versions/8.4.24/manifest.toml)). That manifest is the contract phpflash
reads: the flags, settings, dependencies, fetch scripts and per-mode rules. phpflash's `init` offers
the optional extensions from it, and enabling one in the project config compiles it in. By hand it is
a `-DPHP_EXT_*=ON` flag.

Each extension is in one of three states:

- **Built-in.** Always compiled and registered; nothing to enable.
- **Flag.** Ported but off by default. Turn it on with the given flag (or select it during
  `phpflash init`). Sizes are measured image deltas, see [footprint.md](footprint.md); the porting
  details are in [porting-notes.md](porting-notes.md).
- **Not available.** Not ported. The reason is either an external library it needs that PHP does not
  bundle, an OS feature this hardware and RTOS do not have, or simply that nobody has ported it yet
  (a self-contained extension is a candidate, the way `ctype` and `filter` were before they landed).

The Roadmap column says where each one is headed. **Done** is anything shipped. **Planned** is a
realistic next port, self-contained with no external library or missing OS feature, ranked by value:
next up are the self-contained utilities `bcmath` (arbitrary-precision math) and `phar` (bundling an
app into one archive), then the more situational `exif` (image metadata, useful with the P4's
camera). **Not planned** could be ported but nobody has committed to it; ask if you need one. **Not
possible** relies on something a bare-metal target fundamentally lacks (`fork`, processes, shared
memory, a dynamic loader, an interactive TTY, Windows COM).

## The always-on core

The language itself (the Zend engine) plus these extensions are compiled unconditionally:

`Core` · `standard` · `pcre` · `hash` · `json` · `spl` · `reflection` · `random`

Plus one non-standard extension of ours: **`gpio`** (`gpio_mode`, `gpio_write`, `gpio_read`, `delay`
and the `GPIO_*` constants), covered in [porting-notes.md](porting-notes.md).

## Full list

| Extension | Status | Roadmap | Flag / reason |
|---|---|---|---|
| `bcmath` | Not available | Planned | portable (pure C, arbitrary precision); a self-contained math utility |
| `bz2` | Not available | Not planned | needs libbz2 |
| `calendar` | Not available | Not planned | portable (pure C), but calendar conversion is too niche to prioritize |
| `com_dotnet` | Not available | Not possible | Windows only (COM) |
| `ctype` | **Flag** | Done | `-DPHP_EXT_CTYPE=ON` (~2.5 KB) |
| `curl` | Not available | Not planned | needs libcurl. Networking and TLS exist (esp-tls), but libcurl is not ported; for HTTPS use `openssl` full plus tls and `https://` streams |
| `date` | **Stub + Flag** | Done | a UTC stub is always present; full `DateTime` with `-DPHP_EXT_DATE=ON` (~650 KB, or ~300 KB with `-DPHP_EXT_DATE_MINIMAL_TZ=ON`) |
| `dba` | Not available | Not planned | needs a dbm backend |
| `dom` | Not available | Not planned | needs libxml2 |
| `enchant` | Not available | Not planned | needs enchant/aspell |
| `exif` | Not available | Planned | portable (pure C); niche, image metadata |
| `ffi` | Not available | Not possible | needs libffi and runtime dynamic calls (no dynamic loader here) |
| `fileinfo` | Not available | Not planned | portable but carries the large libmagic database |
| `filter` | **Flag** | Done | `-DPHP_EXT_FILTER=ON` (~27 KB) |
| `ftp` | Not available | Not planned | networking exists; `ext/ftp` not ported |
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
| `mbstring` | **Flag** | Done | `-DPHP_EXT_MBSTRING=ON` (~965 KB; `-DPHP_EXT_MBSTRING_NO_CJK=ON` gives ~209 KB; `-DPHP_EXT_MBSTRING_ONIG=ON` adds `mb_ereg` and `mb_split`, ~445 KB) |
| `mysqli` | Not available | Not planned | needs mysqlnd |
| `mysqlnd` | Not available | Not planned | database client; not ported |
| `oci8` | Not available | Not planned | needs the Oracle client |
| `odbc` | Not available | Not planned | needs unixODBC |
| `opcache` | **Flag** | Done | `-DPHP_EXT_OPCACHE=ON`; a bytecode cache, no JIT. File cache on the microSD by default, in-RAM (PSRAM) with `in_memory`. See [opcache.md](opcache.md) |
| `openssl` | **Flag** | Done | `-DPHP_EXT_OPENSSL=ON` (mbedTLS-backed subset, symmetric AES, ~42 KB); `-DPHP_EXT_OPENSSL_FULL=ON` builds the real OpenSSL 3.0 (RSA/EC/X.509/digests plus on-chip keygen, ~2.1 MB); add `-DPHP_EXT_OPENSSL_TLS=ON` for an HTTPS client. See [openssl.md](openssl.md) |
| `pcntl` | Not available | Not possible | no processes (no `fork`) |
| `pcre` | **Built-in** | Done | always on |
| `pdo` | **Flag** | Done | `-DPHP_EXT_SQLITE=ON` (built together with `pdo_sqlite`) |
| `pdo_dblib` | Not available | Not planned | needs FreeTDS |
| `pdo_firebird` | Not available | Not planned | needs the Firebird client |
| `pdo_mysql` | Not available | Not planned | needs mysqlnd |
| `pdo_oci` | Not available | Not planned | needs the Oracle client |
| `pdo_odbc` | Not available | Not planned | needs unixODBC |
| `pdo_pgsql` | Not available | Not planned | needs libpq |
| `pdo_sqlite` | **Flag** | Done | `-DPHP_EXT_SQLITE=ON` (~560 KB with `pdo`; SQLite amalgamation via `scripts/fetch-sqlite.sh`) |
| `pgsql` | Not available | Not planned | needs libpq |
| `phar` | Not available | Planned | leans on hash and spl; bundle an app into one archive |
| `posix` | Not available | Not possible | no users or processes (the symbols it wants are weak-stubbed instead) |
| `pspell` | Not available | Not planned | needs aspell |
| `random` | **Built-in** | Done | always on |
| `readline` | Not available | Not possible | needs libreadline/editline, and there is no interactive TTY |
| `reflection` | **Built-in** | Done | always on |
| `session` | **Flag** | Done | `-DPHP_EXT_SESSION=ON` (~50 KB): `session_start()` and `$_SESSION`, the files save handler (point `session.save_path` at a writable dir such as the microSD) and user handlers |
| `shmop` | Not available | Not possible | no shared memory |
| `simplexml` | Not available | Not planned | needs libxml2 |
| `snmp` | Not available | Not planned | needs net-snmp |
| `soap` | Not available | Not planned | needs libxml2 |
| `sockets` | Not available | Not planned | lwIP provides BSD sockets (the openssl TLS client uses them), but `ext/sockets` is not ported; a candidate |
| `sodium` | Not available | Not planned | needs libsodium |
| `spl` | **Built-in** | Done | always on |
| `sqlite3` | Not available | Not planned | the standalone `SQLite3` class is not built; use `pdo_sqlite` instead |
| `standard` | **Built-in** | Done | always on (strings, arrays, math, url, base64) |
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

Not listed: `skeleton`, `dl_test` and `zend_test`, which are the source tree's example and test
extensions rather than real features.

## Build flags at a glance

| Flag | Enables | Sub-options |
|---|---|---|
| `-DPHP_EXT_DATE=ON` | real `ext/date` (`DateTime`) | `-DPHP_EXT_DATE_MINIMAL_TZ=ON` (UTC-only tz db) |
| `-DPHP_EXT_CTYPE=ON` | `ext/ctype` | |
| `-DPHP_EXT_FILTER=ON` | `ext/filter` | |
| `-DPHP_EXT_MBSTRING=ON` | `ext/mbstring` | `-DPHP_EXT_MBSTRING_NO_CJK=ON`, `-DPHP_EXT_MBSTRING_ONIG=ON` |
| `-DPHP_EXT_OPENSSL=ON` | `ext/openssl` (mbedTLS subset) | `-DPHP_EXT_OPENSSL_FULL=ON` (real OpenSSL 3.0), `-DPHP_EXT_OPENSSL_NO_LOAD_CONFIG=ON`, `-DPHP_EXT_OPENSSL_TLS=ON` (HTTPS client) |
| `-DPHP_EXT_TOKENIZER=ON` | `ext/tokenizer` | |
| `-DPHP_EXT_SESSION=ON` | `ext/session` | |
| `-DPHP_EXT_SQLITE=ON` | `ext/pdo` and `ext/pdo_sqlite` | |

`phpflash init` offers these interactively and fetches any extra sources on demand. Setting them by
hand, add the flags to `idf.py build`.

Networking is not an extension. It comes with a board that has one (the P4-ETH or the S3-ETH) and is
configured in `php-esp32.config.toml` under `[network]` (static DNS) and `[extensions.openssl]` (the
`tls` and `certs_path` settings for the HTTPS client). See [openssl.md](openssl.md) and the
`https-client` example.
