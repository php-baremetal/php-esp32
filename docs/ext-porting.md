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

## The always-on core

The language itself (the Zend engine) plus these extensions are compiled unconditionally:

`Core` · `standard` · `pcre` · `hash` · `json` · `spl` · `reflection` · `random`

Plus one non-standard extension of ours: **`gpio`** (`gpio_mode`/`gpio_write`/`gpio_read`/
`delay` and the `GPIO_*` constants) — see [porting-notes.md](porting-notes.md).

## Full list

| Extension | Status | Flag / reason |
|---|---|---|
| `bcmath` | Not available | portable (pure C, arbitrary precision) — not yet done |
| `bz2` | Not available | needs libbz2 |
| `calendar` | Not available | portable (pure C) — not yet done |
| `com_dotnet` | Not available | Windows-only (COM) |
| `ctype` | **Flag** | `-DPHP_EXT_CTYPE=ON` (~2.5 KB) |
| `curl` | Not available | needs libcurl + TLS + networking |
| `date` | **Stub + Flag** | a UTC stub is always present; full `DateTime` with `-DPHP_EXT_DATE=ON` (~650 KB, or ~300 KB with `-DPHP_EXT_DATE_MINIMAL_TZ=ON`) |
| `dba` | Not available | needs a dbm backend library |
| `dom` | Not available | needs libxml2 |
| `enchant` | Not available | needs enchant/aspell |
| `exif` | Not available | portable (pure C) — not yet done |
| `ffi` | Not available | needs libffi + runtime dynamic calls |
| `fileinfo` | Not available | portable but carries the large libmagic database — not done |
| `filter` | **Flag** | `-DPHP_EXT_FILTER=ON` (~27 KB) |
| `ftp` | Not available | needs networking |
| `gd` | Not available | needs libpng/libjpeg/freetype |
| `gettext` | Not available | needs libintl |
| `gmp` | Not available | needs libgmp |
| `hash` | **Built-in** | always on |
| `iconv` | Not available | needs libiconv (newlib has no full iconv) |
| `imap` | Not available | needs the c-client library |
| `intl` | Not available | needs ICU (very large) |
| `json` | **Built-in** | always on |
| `ldap` | Not available | needs OpenLDAP |
| `libxml` | Not available | needs libxml2 |
| `mbstring` | **Flag** | `-DPHP_EXT_MBSTRING=ON` (~965 KB; `-DPHP_EXT_MBSTRING_NO_CJK=ON` → ~209 KB; `-DPHP_EXT_MBSTRING_ONIG=ON` adds `mb_ereg*`/`mb_split`, ~445 KB) |
| `mysqli` | Not available | needs mysqlnd + networking |
| `mysqlnd` | Not available | database client, needs networking |
| `oci8` | Not available | needs the Oracle client |
| `odbc` | Not available | needs unixODBC |
| `opcache` | Not available | needs shared memory + writable-executable memory (also no JIT here) |
| `openssl` | Not available | needs OpenSSL (libssl/libcrypto) |
| `pcntl` | Not available | no processes (no `fork`) |
| `pcre` | **Built-in** | always on |
| `pdo` | **Flag** | `-DPHP_EXT_SQLITE=ON` (built together with `pdo_sqlite`) |
| `pdo_dblib` | Not available | needs FreeTDS |
| `pdo_firebird` | Not available | needs the Firebird client |
| `pdo_mysql` | Not available | needs mysqlnd + networking |
| `pdo_oci` | Not available | needs the Oracle client |
| `pdo_odbc` | Not available | needs unixODBC |
| `pdo_pgsql` | Not available | needs libpq |
| `pdo_sqlite` | **Flag** | `-DPHP_EXT_SQLITE=ON` (~560 KB with `pdo`; SQLite amalgamation via `scripts/fetch-sqlite.sh`) |
| `pgsql` | Not available | needs libpq |
| `phar` | Not available | portable-ish (leans on hash/spl) — not yet done |
| `posix` | Not available | no users/processes (the symbols it wants are weak-stubbed instead) |
| `pspell` | Not available | needs aspell |
| `random` | **Built-in** | always on |
| `readline` | Not available | needs libreadline/editline (and there's no interactive TTY) |
| `reflection` | **Built-in** | always on |
| `session` | Not available | portable with a file save-handler — not yet done |
| `shmop` | Not available | no shared memory |
| `simplexml` | Not available | needs libxml2 |
| `snmp` | Not available | needs net-snmp + networking |
| `soap` | Not available | needs libxml2 + networking |
| `sockets` | Not available | no BSD sockets (lwip is only partial) |
| `sodium` | Not available | needs libsodium |
| `spl` | **Built-in** | always on |
| `sqlite3` | Not available | the standalone `SQLite3` class isn't built; use `pdo_sqlite` (`-DPHP_EXT_SQLITE=ON`) instead |
| `standard` | **Built-in** | always on (strings, arrays, math, url, base64, …) |
| `sysvmsg` | Not available | no System V IPC |
| `sysvsem` | Not available | no System V IPC |
| `sysvshm` | Not available | no System V IPC |
| `tidy` | Not available | needs libtidy |
| `tokenizer` | Not available | portable (uses the PHP lexer) — not yet done |
| `xml` | Not available | needs libxml2 |
| `xmlreader` | Not available | needs libxml2 |
| `xmlwriter` | Not available | needs libxml2 |
| `xsl` | Not available | needs libxslt |
| `zip` | Not available | needs libzip |
| `zlib` | Not available | needs zlib |

(Not listed: `skeleton`, `dl_test`, `zend_test` — these are the source tree's example/test
extensions, not real features.)

## Summary of the build flags

| Flag | Enables | Sub-options |
|---|---|---|
| `-DPHP_EXT_DATE=ON` | real `ext/date` (`DateTime`) | `-DPHP_EXT_DATE_MINIMAL_TZ=ON` (UTC-only tz db) |
| `-DPHP_EXT_CTYPE=ON` | `ext/ctype` | — |
| `-DPHP_EXT_FILTER=ON` | `ext/filter` | — |
| `-DPHP_EXT_MBSTRING=ON` | `ext/mbstring` | `-DPHP_EXT_MBSTRING_NO_CJK=ON`, `-DPHP_EXT_MBSTRING_ONIG=ON` |
| `-DPHP_EXT_SQLITE=ON` | `ext/pdo` + `ext/pdo_sqlite` | — |

`./flash.sh` asks about each of these interactively and fetches any extra sources on demand.
