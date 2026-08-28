#!/usr/bin/env bash
# Download and extract the pristine PHP source into components/php/php-<version>/
# (kept out of git; see .gitignore). Use the release tarball, not git: it ships
# the pre-generated lexer and parser, so bison/re2c aren't needed.
#
# The version is per-directory: components/php/versions/<version>/ holds its
# version.env (VERSION + SHA256) and its patches. Which version to fetch defaults
# to `default_version` in php-esp32.toml; override with e.g.
#   PHP_VERSION=8.4.1 ./scripts/fetch-php.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Pick the version: env override, else the repo's default from php-esp32.toml.
if [ -z "${PHP_VERSION:-}" ]; then
    PHP_VERSION="$(grep -oE 'default_version[[:space:]]*=[[:space:]]*"[^"]+"' \
        "${REPO_ROOT}/php-esp32.toml" | grep -oE '"[^"]+"' | tr -d '"')"
fi

VDIR="${REPO_ROOT}/components/php/versions/${PHP_VERSION}"
if [ ! -f "${VDIR}/version.env" ]; then
    echo "!! Unknown PHP version '${PHP_VERSION}': ${VDIR}/version.env not found."
    echo "   Available versions:"; ls "${REPO_ROOT}/components/php/versions" 2>/dev/null | sed 's/^/     /'
    exit 1
fi

# version.env sets PHP_VERSION (authoritative) and PHP_SHA256.
# shellcheck disable=SC1090
source "${VDIR}/version.env"

TARBALL="php-${PHP_VERSION}.tar.gz"
URL="https://www.php.net/distributions/${TARBALL}"
DEST="${REPO_ROOT}/components/php/php-${PHP_VERSION}"

if [ -d "${DEST}" ]; then
    echo "Already present: ${DEST}"
    exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

TARBALL_PATH="${TMP}/${TARBALL}"
if [ -n "${PHP_TARBALL_CACHE:-}" ]; then
    mkdir -p "${PHP_TARBALL_CACHE}"
    TARBALL_PATH="${PHP_TARBALL_CACHE}/${TARBALL}"
fi

if [ -f "${TARBALL_PATH}" ]; then
    echo "Using cached tarball ${TARBALL_PATH}"
else
    echo "Downloading ${TARBALL}..."
    curl -fsSL -o "${TARBALL_PATH}" "${URL}"
fi

echo "Checking sha256..."
echo "${PHP_SHA256}  ${TARBALL_PATH}" | sha256sum -c -

echo "Extracting into ${DEST}..."
tar xzf "${TARBALL_PATH}" -C "${REPO_ROOT}/components/php/"

# Apply this version's patches (port fixes that must live in the vendored source).
# Each patch is a -p1 unified diff rooted at the php-<version> directory.
PATCH_DIR="${VDIR}/patches/php"
if [ -d "${PATCH_DIR}" ]; then
    for patch in "${PATCH_DIR}"/*.patch; do
        [ -e "${patch}" ] || continue
        echo "Applying $(basename "${patch}")..."
        patch -p1 -d "${DEST}" < "${patch}"
    done
fi

# Keep the vendored tree read-only; local changes belong in patches/ instead.
chmod -R a-w "${DEST}"

echo "Done: ${DEST}"
