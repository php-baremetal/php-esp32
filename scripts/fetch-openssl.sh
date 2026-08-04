#!/usr/bin/env bash
# Download and cross-compile OpenSSL (libcrypto) for the ESP32-P4, for the FULL ext/openssl
# build (-DPHP_EXT_OPENSSL=ON -DPHP_EXT_OPENSSL_FULL=ON). Only needed for that build; the default
# openssl (mbedTLS subset) needs nothing here. Produces components/php/openssl-build/{lib,include},
# referenced by versions/<ver>/openssl-full.cmake. Kept out of git (.gitignore). Idempotent.
#
# It builds libcrypto only (no libssl / TLS): crypto-only ext/openssl. The ESP-IDF toolchain must
# be on PATH -- run `. $IDF_PATH/export.sh` first (needs riscv32-esp-elf-gcc).
set -euo pipefail

OPENSSL_VERSION="3.0.15"
TARBALL="openssl-${OPENSSL_VERSION}.tar.gz"
SHA256="23c666d0edf20f14249b3d8f0368acaee9ab585b09e1de82107c66e1f3ec9533"
URL="https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/${TARBALL}"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${REPO_ROOT}/components/php/openssl-src"
OUT="${REPO_ROOT}/components/php/openssl-build"

if [ -f "${OUT}/lib/libcrypto.a" ]; then
    echo "Already present: ${OUT}/lib/libcrypto.a"
    exit 0
fi

if ! command -v riscv32-esp-elf-gcc >/dev/null 2>&1; then
    echo "error: riscv32-esp-elf-gcc not found. Run '. \$IDF_PATH/export.sh' first." >&2
    exit 1
fi

# 1) fetch the source
if [ ! -f "${SRC}/Configure" ]; then
    TMP="$(mktemp -d)"; trap 'rm -rf "${TMP}"' EXIT
    echo "Downloading ${TARBALL}..."
    curl -fsSL -o "${TMP}/${TARBALL}" "${URL}"
    echo "Checking sha256..."
    echo "${SHA256}  ${TMP}/${TARBALL}" | sha256sum -c -
    echo "Extracting into ${SRC}..."
    mkdir -p "${SRC}"
    tar xzf "${TMP}/${TARBALL}" -C "${SRC}" --strip-components=1
fi

# 2) newlib is missing <syslog.h> (OpenSSL's BIO syslog uses it); provide a tiny stub. The BIO
#    syslog itself isn't used by ext/openssl's crypto functions.
SHIM="${SRC}/esp-shim"
mkdir -p "${SHIM}"
cat > "${SHIM}/syslog.h" <<'EOF'
#ifndef _ESP_SYSLOG_STUB_H
#define _ESP_SYSLOG_STUB_H
#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7
#define LOG_PID 0
#define LOG_CONS 0
#define LOG_DAEMON 0
#define LOG_USER 0
static inline void openlog(const char *a, int b, int c) { (void)a;(void)b;(void)c; }
static inline void syslog(int a, const char *b, ...) { (void)a;(void)b; }
static inline void closelog(void) {}
#endif
EOF

# 3) configure for the target: static, no PIC (bare-metal has no dynamic loader / .got.plt), no
#    asm, no threads/sockets/engines. Seed the DRBG from getrandom() -- newlib provides it, backed
#    by esp_fill_random (the hardware RNG) -- so key generation and the provider RNG work. (The
#    firmware also installs a legacy RAND_METHOD in compat/openssl_ssl_stub.c for the legacy path.)
cd "${SRC}"
echo "Configuring OpenSSL ${OPENSSL_VERSION} for esp32-p4 (riscv32, static, no-pic)..."
./Configure linux-generic32 \
    no-asm no-shared no-pic no-threads no-dso no-engine no-tests no-ui-console no-sock \
    no-dgram no-module no-legacy no-secure-memory no-afalgeng no-comp \
    --cross-compile-prefix=riscv32-esp-elf- --with-rand-seed=getrandom \
    -march=rv32imafc_zicsr_zifencei -mabi=ilp32f "-I${SHIM}" -w >/dev/null

echo "Building libcrypto.a (this takes a few minutes)..."
make -j"$(nproc)" build_generated >/dev/null
make -j"$(nproc)" libcrypto.a >/dev/null

# 4) stage libcrypto.a + the generated headers where openssl-full.cmake expects them
echo "Staging into ${OUT}..."
rm -rf "${OUT}"
mkdir -p "${OUT}/lib" "${OUT}/include"
cp "${SRC}/libcrypto.a" "${OUT}/lib/"
cp -r "${SRC}/include/openssl" "${OUT}/include/"
cp -r "${SRC}/include/crypto" "${OUT}/include/" 2>/dev/null || true

echo "Done: $(du -h "${OUT}/lib/libcrypto.a" | cut -f1) libcrypto.a, $(ls "${OUT}/include/openssl" | wc -l) headers."
