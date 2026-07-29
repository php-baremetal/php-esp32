#!/usr/bin/env bash
# Download the SQLite amalgamation (sqlite3.c / sqlite3.h) into
# components/php/sqlite-amalgamation/. Only needed when the optional PDO/SQLite
# extension is enabled; kept out of git (see .gitignore). Idempotent.
set -euo pipefail

SQLITE_YEAR="2024"
SQLITE_ZIP="sqlite-amalgamation-3450300.zip"   # SQLite 3.45.3
SHA256="ea170e73e447703e8359308ca2e4366a3ae0c4304a8665896f068c736781c651"
URL="https://www.sqlite.org/${SQLITE_YEAR}/${SQLITE_ZIP}"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${REPO_ROOT}/components/php/sqlite-amalgamation"

if [ -f "${DEST}/sqlite3.c" ]; then
    echo "Already present: ${DEST}"
    exit 0
fi

if ! command -v unzip >/dev/null 2>&1; then
    echo "!! 'unzip' is required to extract the SQLite amalgamation."
    echo "   Install it (e.g. sudo dnf install unzip) and re-run."
    exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

echo "Downloading ${SQLITE_ZIP}..."
curl -fsSL -o "${TMP}/${SQLITE_ZIP}" "${URL}"

echo "Checking sha256..."
echo "${SHA256}  ${TMP}/${SQLITE_ZIP}" | sha256sum -c -

echo "Extracting into ${DEST}..."
mkdir -p "${DEST}"
unzip -j -o "${TMP}/${SQLITE_ZIP}" '*/sqlite3.c' '*/sqlite3.h' '*/sqlite3ext.h' -d "${DEST}"

# Apply local patches (port fixes to the amalgamation, e.g. FATFS zero-fill).
PATCH_DIR="${REPO_ROOT}/components/php/patches/sqlite"
if [ -d "${PATCH_DIR}" ]; then
    for patch in "${PATCH_DIR}"/*.patch; do
        [ -e "${patch}" ] || continue
        echo "Applying $(basename "${patch}")..."
        patch -p1 -d "${DEST}" < "${patch}"
    done
fi

echo "Done: ${DEST}"
