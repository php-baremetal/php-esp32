#!/usr/bin/env python3
# Check that php-esp32.manifest.toml (the flash-tool contract) stays in sync with the
# actual build: the PHP_EXT_* options in components/php/CMakeLists.txt and the optional
# extension keys in flash.sh. Also validates fetch paths and project-type references.
# Exits non-zero on any mismatch. Requires Python 3.11+ (tomllib).
import os, re, sys

try:
    import tomllib
except ModuleNotFoundError:
    sys.exit("check-manifest: needs Python 3.11+ (tomllib)")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read(p):
    return open(os.path.join(ROOT, p), encoding="utf-8").read()


errors = []

# The manifest is per PHP version; check the repo's default (override with argv[1]).
root_cfg = tomllib.load(open(os.path.join(ROOT, "php-esp32.toml"), "rb"))
version = sys.argv[1] if len(sys.argv) > 1 else root_cfg["default_version"]
vdir = os.path.join("components", "php", "versions", version)
manifest_path = os.path.join(ROOT, vdir, "manifest.toml")
if not os.path.exists(manifest_path):
    sys.exit(f"check-manifest: no manifest for version '{version}' at {vdir}/manifest.toml")

manifest = tomllib.load(open(manifest_path, "rb"))
exts = manifest["extension"]
types = {t["key"] for t in manifest["project_type"]}
storage_types = {s["key"] for s in manifest.get("storage_type", [])}
print(f"checking version {version} ({vdir}/manifest.toml)")

# Each dimension needs at least one selectable (available) option.
if not any(t.get("available") for t in manifest["project_type"]):
    errors.append("no available project_type")
if not any(s.get("available") for s in manifest.get("storage_type", [])):
    errors.append("no available storage_type")

# 1) flags declared in the manifest (extension + settings) vs CMake option()s.
# Boolean flags map to option(FLAG); an enum setting (kind = "enum") maps instead to a
# -DFLAG=<value> backed by a set(FLAG "default" CACHE STRING ...), so it is checked separately.
manifest_flags = set()
enum_flags = set()
for e in exts:
    if "flag" in e:
        manifest_flags.add(e["flag"].split("=")[0])
    for s in e.get("setting", []):
        if s.get("kind") == "enum":
            enum_flags.add(s["flag"].split("=")[0])
            if s.get("default") not in s.get("choices", []):
                errors.append(f"enum setting '{s['key']}' default '{s.get('default')}' is not one of its choices")
        else:
            manifest_flags.add(s["flag"].split("=")[0])

cmake_text = read(os.path.join(vdir, "extensions.cmake"))
cmake_flags = set(re.findall(r"option\((PHP_EXT_[A-Z0-9_]+)", cmake_text))
cmake_enum_flags = set(re.findall(r'set\((PHP_EXT_[A-Z0-9_]+)\s+"[^"]*"\s+CACHE\s+STRING', cmake_text))

for f in sorted(manifest_flags - cmake_flags):
    errors.append(f"flag {f} is in the manifest but not an option() in CMakeLists.txt")
for f in sorted(cmake_flags - manifest_flags):
    errors.append(f"CMake option {f} is not declared in the manifest")
for f in sorted(enum_flags - cmake_enum_flags):
    errors.append(f"enum flag {f} is in the manifest but not a set(... CACHE STRING) in extensions.cmake")
for f in sorted(cmake_enum_flags - enum_flags):
    errors.append(f"CMake set({f} ... CACHE STRING) is not declared as an enum setting in the manifest")

# 1b) project-type flags (e.g. web-server) vs option()s in main/CMakeLists.txt
pt_flags = {t["flag"].split("=")[0] for t in manifest["project_type"] if "flag" in t}
main_opts = set(re.findall(r"option\((PHP_PROJECT_[A-Z_]+)", read(os.path.join("main", "CMakeLists.txt"))))
for f in sorted(pt_flags - main_opts):
    errors.append(f"project-type flag {f} is in the manifest but not an option() in main/CMakeLists.txt")
for f in sorted(main_opts - pt_flags):
    errors.append(f"main/CMakeLists.txt option {f} is not declared as a project-type flag in the manifest")

# 2) optional extension keys vs flash.sh OPTIONAL_EXTS ("key|desc|fetch" lines)
flash = read("flash.sh")
block = re.search(r"OPTIONAL_EXTS=\(\n(.*?)\n\)", flash, re.S)
flash_keys = set(re.findall(r'"\s*([a-z0-9_]+)\s*\|', block.group(1))) if block else set()
manifest_optional = {e["key"] for e in exts if "flag" in e}
for k in sorted(manifest_optional - flash_keys):
    errors.append(f"optional extension '{k}' is in the manifest but not in flash.sh OPTIONAL_EXTS")
for k in sorted(flash_keys - manifest_optional):
    errors.append(f"flash.sh offers '{k}' but the manifest has no optional extension for it")

# 3) fetch scripts exist; required_for references a known project type
for e in exts:
    fetches = [e.get("fetch")] + [s.get("fetch") for s in e.get("setting", [])]
    for f in filter(None, fetches):
        if not os.path.exists(os.path.join(ROOT, f)):
            errors.append(f"extension '{e['key']}' references missing fetch script {f}")
    for t in e.get("required_for", []):
        if t not in types:
            errors.append(f"extension '{e['key']}' required_for unknown project type '{t}'")
    for dep in e.get("requires", []):
        if dep not in {x["key"] for x in exts}:
            errors.append(f"extension '{e['key']}' requires unknown extension '{dep}'")
    # every driver an extension declares must have a source directory + descriptor source
    for drv in e.get("drivers", []):
        p = os.path.join("components", f"php_ext_{e['key']}", "drivers", drv, f"{drv}.c")
        if not os.path.exists(os.path.join(ROOT, p)):
            errors.append(f"extension '{e['key']}' lists driver '{drv}' but {p} is missing")

# 4) chip descriptors: each chip.toml's target must match its sibling sdkconfig.chip, and every
#    preset under it must wire a microSD mode / network type the chip's capabilities allow.
import glob
for ct in glob.glob(os.path.join(ROOT, "boards", "*", "chip.toml")):
    c = tomllib.load(open(ct, "rb"))
    cdir = os.path.dirname(ct)
    rel = os.path.relpath(ct, ROOT)
    sk = os.path.join(cdir, "sdkconfig.chip")
    if c.get("target") and os.path.exists(sk):
        m = re.search(r'CONFIG_IDF_TARGET="([^"]+)"', open(sk).read())
        if m and m.group(1) != c["target"]:
            errors.append(f"{rel}: target '{c['target']}' != sdkconfig.chip '{m.group(1)}'")
    caps = c.get("capabilities", {})
    cap_msd, cap_net = caps.get("microsd", []), caps.get("network", [])
    presets = (glob.glob(os.path.join(cdir, "presets", "*.toml")) +
               glob.glob(os.path.join(cdir, "*", "presets", "*.toml")))
    for pp in presets:
        p = tomllib.load(open(pp, "rb"))
        prel = os.path.relpath(pp, ROOT)
        msd = p.get("storage", {}).get("microsd", {}).get("mode")
        if msd and msd not in cap_msd:
            errors.append(f"{prel}: microsd mode '{msd}' not in chip capabilities {cap_msd}")
        net = p.get("network", {}).get("type")
        if net and net not in cap_net:
            errors.append(f"{prel}: network type '{net}' not in chip capabilities {cap_net}")

if errors:
    print("check-manifest: FAIL")
    for e in errors:
        print("  -", e)
    sys.exit(1)

n_chips = len(glob.glob(os.path.join(ROOT, "boards", "*", "chip.toml")))
n_presets = len(glob.glob(os.path.join(ROOT, "boards", "*", "presets", "*.toml")) +
                glob.glob(os.path.join(ROOT, "boards", "*", "*", "presets", "*.toml")))
print(f"check-manifest: OK ({len(exts)} extensions, {len(storage_types)} storage types, "
      f"{len(types)} project types, {len(manifest_flags)} ext + {len(pt_flags)} project flags "
      f"all match CMake; {n_chips} chip(s), {n_presets} preset(s) validated)")
