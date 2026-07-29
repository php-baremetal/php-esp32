#!/usr/bin/env bash
# Download and extract the pristine PHP source into components/php/php-8.3.32/
# (kept out of git; see .gitignore). Use the release tarball, not git: it ships
# the pre-generated lexer and parser, so bison/re2c aren't needed.
set -euo pipefail

PHP_VERSION="8.3.32"
SHA256="8e1f03eea0b07bc29e1f94d3cfcf0532b0421ec63c1792346b58c3ad8e40fc9b"
TARBALL="php-${PHP_VERSION}.tar.gz"
URL="https://www.php.net/distributions/${TARBALL}"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${REPO_ROOT}/components/php/php-${PHP_VERSION}"

if [ -d "${DEST}" ]; then
    echo "Already present: ${DEST}"
    exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

echo "Downloading ${TARBALL}..."
curl -fsSL -o "${TMP}/${TARBALL}" "${URL}"

echo "Checking sha256..."
echo "${SHA256}  ${TMP}/${TARBALL}" | sha256sum -c -

echo "Extracting into ${DEST}..."
tar xzf "${TMP}/${TARBALL}" -C "${REPO_ROOT}/components/php/"

# Apply local patches (port fixes that must live in the vendored source). Each
# patch is a -p1 unified diff rooted at the php-<version> directory.
PATCH_DIR="${REPO_ROOT}/components/php/patches/php"
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
