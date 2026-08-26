---
eyebrow: 'Docs · Extensions'
lede: 'Every extension in the PHP source tree, and where it stands on this port: which are built in unconditionally, which are optional behind a build flag, which are this project''s own, and which are not available and why. The authoritative, machine-readable list lives in each version''s manifest.toml.'
see_also:
  - href: ./openssl.md
    meta: 'The openssl subset and the full OpenSSL 3.0 build'
  - href: ./opcache.md
    meta: 'Zend OPcache: file cache and in-RAM SHM'
  - href: ./custom-extensions.md
    meta: 'Write your own C extension for a project'
  - href: ../reference/footprint.md
    meta: 'Measured flash and RAM cost of each extension'
prev:
  label: 'Architecture'
  href: ../getting-started/architecture.md
next:
  label: 'OpenSSL'
  href: ./openssl.md
---

# Extension porting status

Every extension that ships in the PHP source tree, and where it stands on this port. There is a
machine-readable version of the optional extensions, per PHP version, in
`components/php/versions/<version>/manifest.toml` (for the current default,
[8.4.24](https://github.com/php-baremetal/php-esp32/blob/master/components/php/versions/8.4.24/manifest.toml)). That manifest is the contract the
`flash-tool` CLI (`phpflash`) reads: the flags, settings, dependencies, fetch scripts and per-mode
rules. It is the single source of truth — `phpflash init` offers the optional extensions from it, and
enabling one in the project config compiles it in. By hand, each is a `-DPHP_EXT_*=ON` flag on
`idf.py build`.

## The three states

Each extension is in exactly one of three states.

| State | Meaning |
|---|---|
| **Built-in** | Always compiled and registered. Nothing to enable, no flag. |
| **Flag** | Ported but off by default. Turn it on with the given flag, or select it during `phpflash init`. Sizes are measured image deltas — see [footprint.md](../reference/footprint.md); the porting details are in [porting-notes.md](../reference/porting-notes.md). |
| **Not available** | Not ported. Either it needs an external library PHP does not bundle, an OS feature this hardware and RTOS lack, or nobody has ported it yet (a self-contained extension is a candidate, the way `ctype` and `filter` were before they landed). |

The **Roadmap** classification says where an unshipped extension is headed:

- **Done** — anything shipped (built-in or flag).
- **Planned** — a realistic next port: self-contained, no external library, no missing OS feature,
  ranked by value. Next up are the self-contained utilities `bcmath` (arbitrary-precision math) and
  `phar` (bundling an app into one archive), then the more situational `exif` (image metadata, useful
  with the P4's camera).
- **Not planned** — could be ported, but nobody has committed to it. Ask if you need one.
- **Not possible** — relies on something a bare-metal target fundamentally lacks: `fork`, processes,
  shared memory, a dynamic loader, an interactive TTY, or Windows COM.

<!-- @callout variant="info" title="The manifest is the contract" -->
`flash-tool` hardcodes nothing about extensions. It reads `manifest.toml` from the installed
php-esp32, presents the choices, records them in `php-esp32.config.toml`, and at build time emits
exactly the `-D<flag>` arguments declared there. When an extension, setting, flag, dependency or
per-type rule changes, it is edited in the manifest and kept in step with the actual build
(`flash.sh` `OPTIONAL_EXTS`, `components/php/CMakeLists.txt` `PHP_EXT_*` options, and
`components/php/internal_functions.c`).
<!-- @endcallout -->

## The always-on core

The language itself (the Zend engine) plus these extensions are compiled unconditionally. They carry
no flag and cannot be turned off:

`Core` · `standard` · `pcre` · `hash` · `json` · `spl` · `reflection` · `random`

`standard` is the bulk of the standard library — strings, arrays, math, `var_dump`, printf, URL,
base64, crypt. `pcre` is the PCRE2 regex engine, `hash` the md5/sha/sha3 family, `spl` the iterators
and data structures, `reflection` the Reflection API, and `random` the `Random\Engine` API with
`random_int()`. Every project type (`init-loop`, `web-server`, `event-driven`) lists all of these as
required.

### The project's own extensions

Three non-standard extensions of ours ship alongside the core. They are built in, but their required
scope differs.

| Extension | Functions | Required for | Notes |
|---|---|---|---|
| `gpio` | `gpio_mode`, `gpio_write`, `gpio_read`, `delay`, and the `GPIO_*` constants | `init-loop`, `event-driven` | Direct pin control. Not required for `web-server`. Covered in [porting-notes.md](../reference/porting-notes.md). |
| `store` | `store_set`, `store_get`, `store_keys`, `store_available`, ... | all types | Reboot-persistent key-value store backed by NVS. Always built in, but needs `[store] size_kb` in the project config to have any flash to use — otherwise `store_available()` is false. See [persistent-store.md](../storage/persistent-store.md). |
| `mem` | `mem_set`, `mem_get`, `mem_keys`, ... | all types | The volatile in-RAM twin of `store`: shares data across the requests of one boot without touching flash. Not persistent across reboots, but cheap to write every request. See [in-ram-store.md](../storage/in-ram-store.md). |

## Full list

Every extension in the upstream source tree. `skeleton`, `dl_test` and `zend_test` are omitted — they
are the source tree's example and test extensions, not real features.

| Extension | Status | Roadmap | Flag / reason |
|---|---|---|---|
| `bcmath` | Not available | Planned | portable (pure C, arbitrary precision); a self-contained math utility |
| `bz2` | Not available | Not planned | needs libbz2 |
| `calendar` | Not available | Not planned | portable (pure C), but calendar conversion is too niche to prioritize |
| `com_dotnet` | Not available | Not possible | Windows only (COM) |
| `ctype` | **Flag** | Done | `-DPHP_EXT_CTYPE=ON` (~2.5 KB) |
| `curl` | Not available | Not planned | needs libcurl. Networking and TLS exist (esp-tls), but libcurl is not ported; for HTTPS use `openssl` full plus `tls` and `https://` streams |
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
| `opcache` | **Flag** | Done | `-DPHP_EXT_OPCACHE=ON`; a bytecode cache, no JIT. File cache on the microSD by default, in-RAM (PSRAM) with `in_memory`. See [opcache.md](./opcache.md) |
| `openssl` | **Flag** | Done | `-DPHP_EXT_OPENSSL=ON` (mbedTLS-backed subset, symmetric AES, ~42 KB); `-DPHP_EXT_OPENSSL_FULL=ON` builds the real OpenSSL 3.0 (RSA/EC/X.509/digests plus on-chip keygen, ~2.1 MB); add `-DPHP_EXT_OPENSSL_TLS=ON` for an HTTPS client. See [openssl.md](./openssl.md) |
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

## Why so much is "not available"

The unported set falls into a few clear buckets. Knowing which one an extension lands in tells you
whether it is worth asking for.

- **Needs an external library PHP does not bundle** — `bz2`, `curl`, `dom`/`libxml`/`simplexml`/`soap`/`xml`/`xmlreader`/`xmlwriter`/`xsl`,
  `gd`, `gettext`, `gmp`, `iconv`, `imap`, `intl` (ICU), `ldap`, `mysqli`/`mysqlnd`/`pdo_mysql`,
  `oci8`/`pdo_oci`, `odbc`/`pdo_odbc`, `pgsql`/`pdo_pgsql`, `pspell`/`enchant`, `snmp`, `sodium`,
  `tidy`, `zip`, `zlib`, and the other `pdo_*` drivers. Porting the library is the real work; the
  extension follows.
- **Depends on an OS feature this target lacks** — `pcntl` (no `fork`), `posix` (no users/processes),
  `shmop` (no shared memory), `sysvmsg`/`sysvsem`/`sysvshm` (no System V IPC), `ffi` (no dynamic
  loader), `readline` (no interactive TTY), `com_dotnet` (Windows only). These are **Not possible**
  on a bare-metal RTOS target, not merely unported.
- **Self-contained and portable, just not done yet** — `bcmath`, `phar`, `exif` (all **Planned**),
  and `calendar`/`fileinfo` (portable but deprioritized). `sockets` is a candidate: lwIP already
  gives BSD sockets, but `ext/sockets` itself is not wired up.

<!-- @callout variant="warning" title="Networking without curl or an HTTP extension" -->
There is no `curl` and no `ext/sockets`, yet the platform does networking. HTTPS from PHP comes from
the full `openssl` build with its `tls` setting: it registers the `ssl://` / `tls://` stream
transport (esp-tls / mbedTLS backed), so `file_get_contents('https://...')` and TLS sockets work
without libcurl. See [openssl.md](./openssl.md) and the `https-client` example.
<!-- @endcallout -->

## Build flags at a glance

Every optional extension and its sub-options. `phpflash init` offers these interactively and fetches
any extra sources on demand; setting them by hand means adding the flags to `idf.py build`.

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

Some flags pull sources that are not committed to the repo. The manifest names a `fetch` script that
must run before the build: `scripts/fetch-sqlite.sh` for the SQLite amalgamation,
`scripts/fetch-oniguruma.sh` for `mbstring`'s `onig` setting, and `scripts/fetch-openssl.sh` for the
full OpenSSL build. `phpflash` runs these automatically when the extension is enabled.

## Enabling an extension in the project config

In a `php-esp32.config.toml`, each optional extension is a `[extensions.<key>]` table with
`enabled = true` and any of its sub-settings. `phpflash` translates these to the `-D` flags above at
build time. A few worked examples follow.

<!-- @tabs labels="mbstring (no CJK),SQLite,date" -->

<!-- @tab index="0" -->

Multibyte strings without the legacy CJK codecs — about 209 KB instead of 965 KB, with UTF-8, UTF-16
and Latin unaffected.

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
[extensions.mbstring]
enabled = true
no_cjk = true
```
<!-- @endcode-block -->

<!-- @endtab -->

<!-- @tab index="1" -->

PDO with the SQLite driver (`ext/pdo` and `ext/pdo_sqlite`, built together). The SQLite amalgamation
is fetched on demand.

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
[extensions.sqlite]
enabled = true
```
<!-- @endcode-block -->

<!-- @endtab -->

<!-- @tab index="2" -->

The real `ext/date` with the full `DateTime` API and the bundled timezone database, replacing the
always-on UTC stub. Add `minimal_tz = true` for a UTC-only tz db (~350 KB smaller, no named zones).

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
[extensions.date]
enabled = true
```
<!-- @endcode-block -->

<!-- @endtab -->

<!-- @endtabs -->

By hand, the equivalent is a flag on the build:

<!-- @code-block language="bash" label="Enabling by hand" -->
```bash
idf.py build -DPHP_EXT_MBSTRING=ON -DPHP_EXT_MBSTRING_NO_CJK=ON
```
<!-- @endcode-block -->

## Notes on the flag extensions

Detail on the extensions that ship behind a flag, beyond the one-line reason in the table.

### date — stub plus full

`date` is unusual: a UTC stub is always present, covering the core date/time functions against UTC.
`-DPHP_EXT_DATE=ON` replaces it with the real `ext/date` — the `DateTime` class and the full API —
at a cost of ~650 KB, about 350 KB of which is the bundled timezone database. The
`minimal_tz` setting ships a UTC-only tz db instead: ~300 KB total, no named zones.

### mbstring — the heavy one

`mbstring` is the largest optional extension, ~965 KB, most of it the CJK conversion tables in the
bundled libmbfl.

| Setting | Effect | Cost |
|---|---|---|
| (base) | `mb_strlen`/`substr`/`convert`/`detect`/… without `mb_ereg` | ~965 KB |
| `no_cjk` | drops the legacy CJK codecs (Shift-JIS, EUC-*, Big5, GB18030, `mb_convert_kana`) | ~209 KB |
| `onig` | adds the `mb_ereg*`/`mb_split` regex family (bundles oniguruma, fetched on demand) | +~445 KB |

UTF-8, UTF-16 and Latin are unaffected by `no_cjk`. The `onig` setting is off by default because most
code does not need multibyte regex, and it is the difference between ~965 KB and ~1.38 MB with full
mbstring.

<!-- @callout variant="info" title="mbstring can back oniguruma, not the other way around" -->
`mb_ereg` and `mb_split` only exist when `onig` is on; plain `mbstring` builds without them. There is
no standalone oniguruma extension — it rides on `mbstring`.
<!-- @endcallout -->

### openssl — subset or full

The default `-DPHP_EXT_OPENSSL=ON` is a small mbedTLS-backed subset (~42 KB): symmetric AES only —
`openssl_encrypt`, `openssl_decrypt`, `openssl_cipher_iv_length`, `openssl_random_pseudo_bytes` —
enough for something like Laravel's `Encrypter`. The `full` setting builds the real `ext/openssl`
against a ported OpenSSL 3.0 libcrypto instead (~2.1 MB): RSA, EC, X.509, EVP, digests, and on-chip
keygen. The `tls` setting (full build only) adds the `ssl://`/`tls://` stream transport for an HTTPS
client, and `no_load_config` skips reading `openssl.cnf` at startup. Full detail in
[openssl.md](./openssl.md).

### opcache — bytecode cache, no JIT

`-DPHP_EXT_OPCACHE=ON` (~500 KB) caches compiled bytecode so each request skips re-tokenizing and
recompiling the sources — the per-request cost for a framework is compilation, so this is a large
win. The default is a file cache on the microSD (bytecode on the card, full PSRAM free for the
request). The `in_memory` setting keeps the cache in PSRAM (SHM) instead, fastest once warm, but the
whole bytecode plus per-request heap must fit in PSRAM — good for a small app, not a large framework.
See [opcache.md](./opcache.md).

### pdo / sqlite

`-DPHP_EXT_SQLITE=ON` builds `ext/pdo` and `ext/pdo_sqlite` together, ~560 KB (about 530 KB of that
is the SQLite library itself, fetched via `scripts/fetch-sqlite.sh`). The standalone `SQLite3` class
is not built — use the PDO driver.

### session, tokenizer, ctype, filter

The small, self-contained ones. `session` (~50 KB) gives `session_start()`, `$_SESSION`,
`session_id()` and save handlers — the files handler (point `session.save_path` at a writable dir
such as the microSD) and user handlers via `session_set_save_handler()`. `tokenizer` (~13 KB) exposes
`token_get_all()`, `token_name()`, the `PhpToken` class and the `T_*` constants over the engine's own
lexer, wanted by frameworks and static-analysis tools. `ctype` (~2.5 KB, one source file, no data
tables) and `filter` (~27 KB, `filter_var()` validation and sanitization) are pure additions with no
sub-options.

## Networking is not an extension

Networking does not appear in the extension list. It comes with a board that has a network (the
P4-ETH or the S3-ETH) and is configured in `php-esp32.config.toml` under `[network]` (static DNS) and
`[extensions.openssl]` (the `tls` and `certs_path` settings for the HTTPS client). The `web-server`
project type adds an HTTP server (`esp_http_server`) in front, invoking PHP fresh per request. See
[openssl.md](./openssl.md) and the `https-client` example.

## Writing your own

If the extension you need is not here and not portable as a straight PHP source-tree extension, a
project can ship its own C extension — compiled into the firmware alongside the core, with its own
functions and constants exposed to PHP. See [custom-extensions.md](./custom-extensions.md).
