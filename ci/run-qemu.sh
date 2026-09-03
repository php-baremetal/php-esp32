#!/usr/bin/env bash
# Boot a built firmware image under Espressif's QEMU (headless) and assert the PHP banner appears
# on the serial output. A boot regression -- PSRAM not coming up, the engine not starting -- shows
# as a missing banner and fails the job, all without hardware.

set -euo pipefail

build_dir="${1:-build}"
expect="${2:-on ESP32-S3}"

{
  echo 'CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=n'
  echo 'CONFIG_ESP_CONSOLE_UART_DEFAULT=y'
  echo 'CONFIG_ESP_CONSOLE_UART_NUM=0'
} >> "$build_dir/sdkconfig"
idf.py -B "$build_dir" build >/dev/null

merge_args=$(python3 - "$build_dir/flasher_args.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
print(" ".join(f"{off} {f}" for off, f in d["flash_files"].items()))
PY
)
( cd "$build_dir" && esptool.py --chip=esp32s3 merge_bin --output=qemu_flash.bin \
    --fill-flash-size=4MB --flash_mode dio --flash_freq 80m --flash_size 4MB $merge_args >/dev/null )

qemu=$(ls "$HOME"/.espressif/tools/qemu-xtensa/*/qemu/bin/qemu-system-xtensa 2>/dev/null | head -1)
if [ -z "$qemu" ]; then
  echo "::error::qemu-system-xtensa not found. Run: python \$IDF_PATH/tools/idf_tools.py install qemu-xtensa"
  exit 1
fi

serial=$(mktemp)

timeout 60 "$qemu" -M esp32s3 -m 32M \
  -drive file="$build_dir/qemu_flash.bin",if=mtd,format=raw \
  -global driver=timer.esp32s3.timg,property=wdt_disable,value=true \
  -nographic -serial file:"$serial" >/dev/null 2>&1 || true

echo "=== QEMU serial output ==="
sed 's/\x1b\[[0-9;]*m//g' "$serial"
echo "=========================="

if grep -qa "$expect" "$serial"; then
  echo "QEMU smoke test OK: found '$expect' on the serial output"
else
  echo "::error::QEMU smoke test failed: '$expect' not found (PSRAM or engine did not come up)"
  exit 1
fi
