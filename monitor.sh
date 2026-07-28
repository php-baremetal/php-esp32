#!/usr/bin/env bash
# Open the serial monitor only (no build, no flash). Asks which serial port to use.
#
# Usage: ./monitor.sh
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

echo "==> Monitoring ${PORT}  (Ctrl-] to quit)"
idf.py -p "${PORT}" monitor
