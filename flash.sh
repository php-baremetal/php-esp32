#!/usr/bin/env bash
# Build and flash the board. Asks which serial port to use and, at the end,
# whether to open the monitor.
#
# Usage: ./flash.sh
# Set IDF_PATH to point at ESP-IDF somewhere other than ~/esp/esp-idf.
set -euo pipefail

IDF_DIR="${IDF_PATH:-${HOME}/esp/esp-idf}"
REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"

if [ ! -f "${IDF_DIR}/export.sh" ]; then
    echo "ESP-IDF not found in ${IDF_DIR}."
    echo "Run ./setup.sh first (or set IDF_PATH)."
    exit 1
fi

# load the ESP-IDF environment into this shell
# shellcheck disable=SC1091
source "${IDF_DIR}/export.sh" >/dev/null

cd "${REPO_ROOT}"

# list available serial ports
mapfile -t PORTS < <(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true)
if [ "${#PORTS[@]}" -eq 0 ]; then
    echo "No serial port found (/dev/ttyACM* or /dev/ttyUSB*)."
    echo "Plug the board in over USB-C and try again."
    exit 1
fi

if [ "${#PORTS[@]}" -eq 1 ]; then
    PORT="${PORTS[0]}"
    echo "Using the only port found: ${PORT}"
else
    echo "Which serial port?"
    select PORT in "${PORTS[@]}"; do
        [ -n "${PORT:-}" ] && break
        echo "Invalid choice, try again."
    done
fi

# permissions: if the port isn't readable/writable, try to unlock it
if [ ! -r "${PORT}" ] || [ ! -w "${PORT}" ]; then
    echo "Port ${PORT} is not accessible; trying to unlock it (needs sudo)..."
    sudo chmod a+rw "${PORT}" || {
        echo "Could not unlock it. Add yourself to the dialout group:"
        echo "   sudo usermod -aG dialout \$USER   (then log out and back in)"
        exit 1
    }
fi

# Optional PHP extensions, kept out of the default build. Each entry is
# "key|description|fetch-script" (fetch-script may be empty). Answering yes maps
# to -DPHP_EXT_<KEY>=ON passed to the build; no maps to =OFF (so toggling off
# works too). Add more extensions by adding a line here plus the matching blocks
# in components/php/CMakeLists.txt and internal_functions.c.
OPTIONAL_EXTS=(
    "date|ext/date: real DateTime and date/time functions|"
    "ctype|ext/ctype: character-class checks (ctype_*)|"
    "mbstring|ext/mbstring: multibyte strings (mb_*), no mb_ereg|"
    "filter|ext/filter: filter_var() validation and sanitization|"
    "tokenizer|ext/tokenizer: token_get_all()/PhpToken over the PHP lexer|"
    "session|ext/session: session_start()/\$_SESSION (files + user handlers)|"
    "openssl|ext/openssl: openssl_* (mbedTLS subset by default; symmetric AES)|"
    "sqlite|PDO + SQLite: read/write .db files on the microSD|scripts/fetch-sqlite.sh"
    "opcache|Zend OPcache: file-cache bytecode cache (no JIT/SHM); needs a writable microSD|"
    "wifi|wifi: scan/join/create a WiFi network from PHP (WiFi-capable SoCs, not the ESP32-P4)|"
    "s3_onboard_rgb|s3_onboard_rgb: drive the onboard WS2812 RGB LED (ESP32-S3 boards only)|"
)

EXT_ARGS=()
DATE_ON=0
MBSTRING_ON=0
OPENSSL_ON=0
echo "Optional PHP extensions (off by default):"
for entry in "${OPTIONAL_EXTS[@]}"; do
    IFS='|' read -r key desc fetch <<< "${entry}"
    key_upper="$(printf '%s' "${key}" | tr '[:lower:]' '[:upper:]')"
    read -r -p "  Include ${key} (${desc})? [y/N] " ans
    case "${ans}" in
        y|Y|s|S)
            if [ -n "${fetch}" ]; then
                # fetch the extra source on demand (idempotent, git-ignored)
                "${REPO_ROOT}/${fetch}"
            fi
            EXT_ARGS+=("-DPHP_EXT_${key_upper}=ON")
            [ "${key}" = "date" ] && DATE_ON=1
            [ "${key}" = "mbstring" ] && MBSTRING_ON=1
            [ "${key}" = "openssl" ] && OPENSSL_ON=1
            ;;
        *)
            EXT_ARGS+=("-DPHP_EXT_${key_upper}=OFF")
            ;;
    esac
done

# Sub-options of ext/mbstring.
MBSTRING_NO_CJK=OFF
MBSTRING_ONIG=OFF
if [ "${MBSTRING_ON}" = "1" ]; then
    read -r -p "    mbstring: drop CJK encodings (~740 KB smaller, but no Shift-JIS/EUC/Big5)? [y/N] " ans
    case "${ans}" in y|Y|s|S) MBSTRING_NO_CJK=ON ;; esac
    read -r -p "    mbstring: build mb_ereg*/mb_split regex (bundles oniguruma)? [y/N] " ans
    case "${ans}" in
        y|Y|s|S)
            "${REPO_ROOT}/scripts/fetch-oniguruma.sh"   # fetch the regex library on demand
            MBSTRING_ONIG=ON
            ;;
    esac
fi
EXT_ARGS+=("-DPHP_EXT_MBSTRING_NO_CJK=${MBSTRING_NO_CJK}")
EXT_ARGS+=("-DPHP_EXT_MBSTRING_ONIG=${MBSTRING_ONIG}")

# Sub-option of ext/openssl: the real ext/openssl on a ported OpenSSL instead of the subset.
OPENSSL_FULL=OFF
if [ "${OPENSSL_ON}" = "1" ]; then
    read -r -p "    openssl: build the FULL real ext/openssl (ported OpenSSL, ~2 MB) instead of the mbedTLS subset? [y/N] " ans
    case "${ans}" in
        y|Y|s|S)
            "${REPO_ROOT}/scripts/fetch-openssl.sh"   # download + cross-compile libcrypto (slow, one-time)
            OPENSSL_FULL=ON
            ;;
    esac
fi
EXT_ARGS+=("-DPHP_EXT_OPENSSL_FULL=${OPENSSL_FULL}")

# Sub-option of ext/date: ship only UTC instead of the full ~350 KB timezone database.
MINIMAL_TZ=OFF
if [ "${DATE_ON}" = "1" ]; then
    read -r -p "    date: UTC-only timezone db (smaller, but no named zones like Europe/Rome)? [y/N] " ans
    case "${ans}" in y|Y|s|S) MINIMAL_TZ=ON ;; esac
fi
EXT_ARGS+=("-DPHP_EXT_DATE_MINIMAL_TZ=${MINIMAL_TZ}")

echo "==> Flashing ${PORT}"
idf.py "${EXT_ARGS[@]}" -p "${PORT}" flash

read -r -p "Open the serial monitor on ${PORT}? [y/N] " ans
case "${ans}" in
    y|Y|s|S)
        echo "(Ctrl-] to quit the monitor)"
        idf.py -p "${PORT}" monitor
        ;;
    *)
        echo "OK. To open it later:  idf.py -p ${PORT} monitor"
        ;;
esac
