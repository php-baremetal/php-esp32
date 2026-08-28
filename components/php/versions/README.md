# PHP versions

Each supported PHP version is a self-contained directory, `components/php/versions/<version>/`. Pick
one with `-DPHP_VERSION=<ver>`; the default is `default_version` in the repo-root
[`php-esp32.toml`](../../../php-esp32.toml). `./scripts/info.sh` lists them.

PHP **8.3** (8.3.33), **8.4** (8.4.25) and **8.5** (8.5.10) are all supported, selectable with `-DPHP_VERSION=<ver>` (default `8.4.25`). The layout keeps everything
version-specific in one directory so further releases slot in beside it as they are ported, without
touching the shared engine glue or the boards.

One 8.5-specific note: 8.5 makes `ext/uri` a core dependency, with two parser backends. This port
builds the RFC 3986 parser (uriparser) and the legacy `parse_url()` parser -- what the stream layer
actually uses -- but drops the WHATWG backend, whose lexbor tables (~370 KB of static data) do not fit
the ESP32's internal RAM. `parse_url()`, `FILTER_VALIDATE_URL` and `Uri\Rfc3986\Uri` all work;
`Uri\WhatWg\Url` registers but raises on construction. See `versions/8.5.10/uri.cmake`.

## What a version owns

- `version.env`: the tarball version and sha256 (read by `scripts/fetch-php.sh`).
- `sources.cmake`: the source-file list and include dirs. The file set differs between PHP releases,
  so it lives per version.
- `extensions.cmake`: the optional-extension wiring (the `PHP_EXT_*` options and their sources).
- `php_config.h`, `zend_config.h`, `build-defs.h`, `main/php_config.h`, `internal_functions.c`: the
  hand-written config and glue for that version.
- `compat/`: the version-sensitive compat (`date_stub.c`, `timelib_config.h`,
  `timezonedb_minimal.h`, which track timelib).
- `patches/php/`: patches applied to the vendored source by `fetch-php.sh`.
- `manifest.toml`: the extension manifest phpflash reads for this version.

Version-agnostic pieces stay shared at the component root: the platform `compat/` (`posix_stubs.c`,
`syslog.*`, `sqlite-compat.h`, `sys/`) and the external dependencies (the SQLite amalgamation and
oniguruma, which track their own upstreams).

## Add a version

1. `mkdir components/php/versions/<newver>/` and populate it by copying `8.3.33/` and adjusting.
   Regenerate `sources.cmake` from the new tarball's file list, re-check the hand-written headers, and
   confirm the patches still apply against the new source (or update them).
2. Put the version and sha256 in `version.env`, and write its `manifest.toml`.
3. `PHP_VERSION=<newver> ./scripts/fetch-php.sh` to download and patch, then build with
   `idf.py -DPHP_VERSION=<newver>` (or set it in a project's config and use phpflash).
4. `./scripts/check-manifest.py <newver>` validates that version's manifest.
