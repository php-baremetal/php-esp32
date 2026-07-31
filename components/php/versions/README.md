# PHP versions

Each supported PHP version is a self-contained directory,
`components/php/versions/<version>/`. Pick one with `-DPHP_VERSION=<ver>`; the default is
`default_version` in the repo-root [`php-esp32.toml`](../../../php-esp32.toml).
`./scripts/info.sh` lists them.

## What a version owns

- `version.env` — the tarball version + sha256 (read by `scripts/fetch-php.sh`).
- `sources.cmake` — the source-file list + include dirs (the file set differs between PHP
  versions, so it lives per version).
- `extensions.cmake` — the optional-extension wiring (the `PHP_EXT_*` options and their sources).
- `php_config.h`, `zend_config.h`, `build-defs.h`, `main/php_config.h`, `internal_functions.c` —
  the hand-written config/glue for that version.
- `compat/` — the version-sensitive compat (`date_stub.c`, `timelib_config.h`,
  `timezonedb_minimal.h`, which track timelib).
- `patches/php/` — patches applied to the vendored source by `fetch-php.sh`.
- `manifest.toml` — the extension manifest flash-tool reads for this version.

Version-agnostic pieces stay shared at the component root: the platform `compat/`
(`posix_stubs.c`, `syslog.*`, `sqlite-compat.h`, `sys/`) and the external dependencies (the
SQLite amalgamation and oniguruma, which track their own upstreams).

## Add a version

1. `mkdir components/php/versions/<newver>/` and populate it — copy `8.3.32/` and adjust.
   Regenerate `sources.cmake` from the new tarball's file list; re-check the hand-written
   headers, and that the patches still apply against the new source (or update them).
2. Put the version + sha256 in `version.env`; write its `manifest.toml`.
3. `PHP_VERSION=<newver> ./scripts/fetch-php.sh` to download + patch, then build with
   `idf.py -DPHP_VERSION=<newver> ...`.
4. `./scripts/check-manifest.py <newver>` validates that version's manifest.
