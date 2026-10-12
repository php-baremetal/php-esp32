#!/usr/bin/env bash
# Show what this php-esp32 checkout can build: the default version/board, the PHP versions available,
# and -- per board preset -- the modes it offers, i.e. what the firmware IMPLEMENTS (the version
# manifest) intersected with what the preset WIRES (its peripherals, over the chip's capabilities).
#
# It also documents, by example, how flash-tool discovers this repo:
#   - defaults      : php-esp32.toml (default_version, default_board)
#   - PHP versions  : components/php/versions/*/manifest.toml
#   - chips/presets : boards/*/chip.toml and boards/*/presets/*.toml (+ boards/*/*/presets/*.toml)
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

exec python3 - "$REPO_ROOT" <<'PY'
import sys, os, glob, tomllib
root = sys.argv[1]

def load(p): return tomllib.load(open(p, "rb"))

cfg = load(os.path.join(root, "php-esp32.toml"))
print("php-esp32")
print(f"  default version : {cfg['default_version']}")
print(f"  default board   : {cfg['default_board']}")

vers = sorted(os.path.basename(os.path.dirname(p))
              for p in glob.glob(os.path.join(root, "components/php/versions/*/manifest.toml")))
print(f"\nPHP versions ({len(vers)}):")
for v in vers:
    m = load(os.path.join(root, "components/php/versions", v, "manifest.toml"))
    opt = [e["key"] for e in m["extension"] if "flag" in e]
    print(f"  {v}   optional extensions: {', '.join(opt)}")

# The default version's manifest says what modes the firmware implements.
m = load(os.path.join(root, "components/php/versions", cfg["default_version"], "manifest.toml"))
impl_storage = {s["key"] for s in m.get("storage_type", []) if s.get("available")}
impl_project = {t["key"] for t in m["project_type"] if t.get("available")}

def net_kind(preset_net, chip_net):
    # Mirror flash-tool's netKind: the chip's on-die radio is always available (the `wifi` extension),
    # plus a wired PHY when the preset wires one; a companion-WiFi preset reports wifi.
    has_wifi = "wifi" in chip_net
    wired = preset_net not in (None, "", "wifi-c6")
    wifi = has_wifi or preset_net == "wifi-c6"
    if wired and wifi: return "ethernet+wifi"
    if wired:          return "ethernet"
    if wifi:           return "wifi"
    return "none"

print("\nChips and presets (offered = firmware-implemented ∩ preset-wired):")
for ct in sorted(glob.glob(os.path.join(root, "boards/*/chip.toml"))):
    chip = load(ct)
    cdir = os.path.dirname(ct)
    ckey = os.path.basename(cdir)
    chip_net = chip.get("capabilities", {}).get("network", [])
    print(f"  {ckey}  ({chip.get('name','?')}, target {chip.get('target','?')})")
    presets = (sorted(glob.glob(os.path.join(cdir, "presets", "*.toml"))) +
               sorted(glob.glob(os.path.join(cdir, "*", "presets", "*.toml"))))
    for pp in presets:
        p = load(pp)
        key = os.path.splitext(os.path.basename(pp))[0]
        # The build key: "chip" for a mode-less chip, "chip/mode" when the preset sits under a mode leaf.
        mode = os.path.basename(os.path.dirname(os.path.dirname(pp)))
        build_key = ckey if mode == ckey else f"{ckey}/{mode}"
        sel = f'family = "{build_key}"' if key == "generic" else f'preset = "{key}"'
        msd = p.get("storage", {}).get("microsd", {}).get("mode")
        net = p.get("network", {}).get("type")
        network = net_kind(net, chip_net)
        storage_types = ["microsd", "embedded"] if msd else ["embedded"]
        project_types = ["init-loop", "event-driven"] + (["web-server"] if network != "none" else [])
        off_s = [s for s in storage_types if s in impl_storage]
        off_p = [t for t in project_types if t in impl_project]
        print(f"    [{build_key}]  {sel}")
        print(f"      storage: {', '.join(off_s) or '-'}    project: {', '.join(off_p) or '-'}"
              f"    network: {network}    microSD: {'yes' if msd else 'no'}")
PY
