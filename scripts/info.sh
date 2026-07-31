#!/usr/bin/env bash
# Show what this php-esp32 checkout can build: the default version/board, the PHP
# versions and boards available, and -- per board -- the modes it offers, i.e. what
# the firmware IMPLEMENTS (the version manifest) intersected with what the board's
# HARDWARE supports (its board.toml).
#
# It also documents, by example, how flash-tool discovers this repo:
#   - defaults          : php-esp32.toml (default_version, default_board)
#   - PHP versions       : components/php/versions/*/manifest.toml
#   - families / boards  : boards/*/family.toml and boards/*/*/board.toml
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

print("\nBoards (offered = firmware-implemented ∩ board-supported):")
for bt in sorted(glob.glob(os.path.join(root, "boards/*/*/board.toml"))):
    b = load(bt)
    fam = os.path.basename(os.path.dirname(os.path.dirname(bt)))
    brd = os.path.basename(os.path.dirname(bt))
    off_s = [s for s in b.get("storage_types", []) if s in impl_storage]
    off_p = [t for t in b.get("project_types", []) if t in impl_project]
    print(f"  {fam}/{brd}  ({b.get('name', '?')})")
    print(f"    storage: {', '.join(off_s) or '-'}    project: {', '.join(off_p) or '-'}")
PY
