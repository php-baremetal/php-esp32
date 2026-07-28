#!/usr/bin/env bash
# Install everything needed to build this project:
#   - system prerequisites
#   - ESP-IDF with the riscv32-esp-elf toolchain
#   - the PHP source (downloaded and verified)
#
# Usage: ./setup.sh
# Set IDF_PATH to install ESP-IDF somewhere other than ~/esp/esp-idf.
set -euo pipefail

IDF_VERSION="v5.5.5"
IDF_DIR="${IDF_PATH:-${HOME}/esp/esp-idf}"
REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"

echo "==> System prerequisites"
if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv \
        cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y git wget flex bison gperf python3 python3-pip cmake ninja-build \
        ccache dfu-util libusbx
elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -S --needed --noconfirm git wget flex bison gperf python cmake ninja \
        ccache dfu-util libusb
else
    echo "!! Unknown package manager. Install the ESP-IDF prerequisites by hand:"
    echo "   git wget flex bison gperf python3 cmake ninja ccache dfu-util libusb"
fi

echo "==> ESP-IDF ${IDF_VERSION} in ${IDF_DIR}"
if [ -d "${IDF_DIR}/.git" ]; then
    echo "   already present, skipping clone."
else
    mkdir -p "$(dirname "${IDF_DIR}")"
    git clone -b "${IDF_VERSION}" --depth 1 --shallow-submodules --recursive \
        https://github.com/espressif/esp-idf.git "${IDF_DIR}"
fi

echo "==> Toolchain for esp32p4 (downloads a few hundred MB)"
"${IDF_DIR}/install.sh" esp32p4

echo "==> PHP source"
"${REPO_ROOT}/scripts/fetch-php.sh"

cat <<EOF

Done. To build:

    source ${IDF_DIR}/export.sh
    idf.py set-target esp32p4     # first time only
    idf.py build

To flash the board:  ./flash.sh
EOF
