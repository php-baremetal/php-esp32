#!/usr/bin/env bash
# Fail if the built app image outgrows the board's `factory` app partition.
# The cap is read straight from the board's committed partitions.csv, so it tracks
# the real limit and never drifts from a hand-maintained number. On the S3-Zero
# (factory = 3456K) a size regression is a functional bug, not a cosmetic one.
set -euo pipefail

board="$1"
build_dir="${BUILD_DIR:-build}"
# The `factory` cap is read from the partition table the build actually used. That table is
# generated per build (cmake/gen-partitions.cmake, sized to fill the flash) and its absolute path is
# recorded in the generated sdkconfig. ESP-IDF writes sdkconfig in the project dir (not build/), so
# look there first, then inside the build dir.
csv=""
for sdkconfig in "sdkconfig" "${build_dir}/sdkconfig" "${build_dir}/../sdkconfig"; do
  [ -f "$sdkconfig" ] || continue
  csv=$(sed -nE 's/^CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="(.*)"$/\1/p' "$sdkconfig")
  [ -n "$csv" ] && break
done
if [ -z "$csv" ] || [ ! -f "$csv" ]; then
  # Fallback: the chip's committed table (boards/<chip>/partitions.csv).
  csv=$(ls "boards/${board%%/*}/partitions.csv" 2>/dev/null | head -1)
fi
cap=$(awk -F',' '/^[[:space:]]*#/ { next } { gsub(/[[:space:]]/,"",$1) } $1 == "factory" { gsub(/[[:space:]]/,"",$5); print $5 }' "$csv")

to_bytes() {
  case "$1" in
    *K|*k) echo $(( ${1%[Kk]} * 1024 )) ;;
    *M|*m) echo $(( ${1%[Mm]} * 1024 * 1024 )) ;;
    *)     echo "$1" ;;
  esac
}

cap_b=$(to_bytes "$cap")
app_bin="${build_dir}/$(python3 -c "import json; print(json.load(open('${build_dir}/project_description.json'))['app_bin'])")"
size_b=$(stat -c%s "$app_bin")
pct=$(( size_b * 100 / cap_b ))

echo "$board: image ${size_b} B / factory ${cap} (${cap_b} B) = ${pct}%"
if [ "$size_b" -gt "$cap_b" ]; then
  echo "::error::$board app image ${size_b} B exceeds the factory partition (${cap} = ${cap_b} B)"
  exit 1
fi
