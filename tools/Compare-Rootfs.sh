#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
while (($#)); do
    case "$1" in
        -ProjectDir|--project-dir) (($# >= 2)) || { echo "missing value for $1" >&2; exit 2; }; PROJECT_DIR=$2; shift 2 ;;
        -h|--help) echo 'Usage: Compare-Rootfs.sh [-ProjectDir DIR]'; exit 0 ;;
        *) printf 'unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done
PROJECT_DIR=$(cd -- "$PROJECT_DIR" && pwd)
source "$SCRIPT_DIR/Resolve-SystemTool.sh"
PYTHON=$(resolve_system_tool python.exe)
mkdir -p "$PROJECT_DIR/temp/manifests"
PROJECT_DIR="$PROJECT_DIR" "$PYTHON" - <<'PY'
import csv
import hashlib
import os
from pathlib import Path

project = Path(os.environ["PROJECT_DIR"])
roots = {
    "Factory": project / "temp/rootfs-factory-system0",
    "Patched": project / "temp/rootfs-patched",
}
index = {}
for side, root in roots.items():
    if not root.is_dir():
        raise SystemExit(f"rootfs directory not found: {root}")
    for current, dirs, files in os.walk(root, followlinks=False):
        dirs[:] = [name for name in dirs if not (Path(current) / name).is_symlink()]
        for name in files:
            full = Path(current) / name
            if full.is_symlink() or not full.is_file():
                continue
            relative = full.relative_to(root).as_posix()
            digest = hashlib.sha256()
            size = 0
            with full.open("rb") as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    size += len(block)
                    digest.update(block)
            index.setdefault(relative, {})[side] = (size, digest.hexdigest().upper())

columns = ["Path", "State", "FactoryBytes", "PatchedBytes", "FactorySHA256", "PatchedSHA256"]
rows = []
for relative in sorted(index, key=str.casefold):
    factory = index[relative].get("Factory")
    patched = index[relative].get("Patched")
    state = "Added" if factory is None else "Removed" if patched is None else "Changed" if factory[1] != patched[1] else "Same"
    rows.append({
        "Path": relative, "State": state,
        "FactoryBytes": factory[0] if factory else "", "PatchedBytes": patched[0] if patched else "",
        "FactorySHA256": factory[1] if factory else "", "PatchedSHA256": patched[1] if patched else "",
    })

def export(path, output_rows):
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns, extrasaction="ignore", lineterminator="\r\n", quoting=csv.QUOTE_ALL)
        writer.writeheader()
        writer.writerows(output_rows)

manifest_dir = project / "temp/manifests"
export(manifest_dir / "rootfs-file-manifest.csv", rows)
differences = [row for row in rows if row["State"] != "Same"]
export(manifest_dir / "rootfs-differences.csv", differences)
counts = {}
for row in rows:
    counts[row["State"]] = counts.get(row["State"], 0) + 1
for state in sorted(counts, key=str.casefold):
    print(f"{state} {counts[state]}")
for row in differences:
    print("\t".join(str(row[key]) for key in ("Path", "State", "FactoryBytes", "PatchedBytes")))
PY
