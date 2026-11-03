#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
FORCE=0
while (($#)); do
    case "$1" in
        -ProjectDir|--project-dir) (($# >= 2)) || { echo "missing value for $1" >&2; exit 2; }; PROJECT_DIR=$2; shift 2 ;;
        -Force|--force) FORCE=1; shift ;;
        -h|--help) echo 'Usage: Fetch-Inputs.sh [-ProjectDir DIR] [-Force]'; exit 0 ;;
        *) printf 'unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done
PROJECT_DIR=$(cd -- "$PROJECT_DIR" && pwd)
command -v curl >/dev/null 2>&1 || { echo 'Required system tool curl was not found on PATH.' >&2; exit 1; }
if command -v sha256sum >/dev/null 2>&1; then
    file_sha256() { sha256sum "$1" | awk '{print tolower($1)}'; }
elif command -v shasum >/dev/null 2>&1; then
    file_sha256() { shasum -a 256 "$1" | awk '{print tolower($1)}'; }
else
    echo 'Required system tool sha256sum or shasum was not found on PATH.' >&2
    exit 1
fi

base=https://github.com/idootop/open-xiaoai/releases/download/LX06_1.94.13
dest=$PROJECT_DIR/rom
mkdir -p "$dest"
names=(LX06_1.94.13.squashfs LX06_1.94.13_patched.squashfs)
hashes=(317a54909f12b63f22128965bf9005c06435d15f61c4202bb88c4524bf1e2ef4 3fbcfcd8a2130f5421d0bc3a3b22cb5a3de59b5d3678a3a14b23541aa9474225)
CURRENT_PART=
cleanup_partial() { [[ -z $CURRENT_PART ]] || rm -f -- "$CURRENT_PART"; }
trap cleanup_partial EXIT
trap 'exit 1' HUP INT TERM
for i in 0 1; do
    name=${names[$i]}
    expected=${hashes[$i]}
    path=$dest/$name
    part=$path.part
    if [[ -f $path ]] && (( ! FORCE )); then
        have=$(file_sha256 "$path")
        if [[ $have == "$expected" ]]; then
            printf 'present %s\n' "$name"
            continue
        fi
        printf 'hash_mismatch %s expected=%s actual=%s\n' "$name" "$expected" "$have"
    fi
    url=$base/$name
    printf 'download %s\n' "$url"
    CURRENT_PART=$part
    rm -f -- "$part"
    if ! curl --fail --location --silent --show-error --proto '=https' --tlsv1.2 --output "$part" "$url"; then
        rm -f -- "$part"
        echo "download failed for $name" >&2
        exit 1
    fi
    got=$(file_sha256 "$part")
    if [[ $got != "$expected" ]]; then
        rm -f -- "$part"
        printf 'sha256 mismatch for %s: expected %s, got %s\n' "$name" "$expected" "$got" >&2
        exit 1
    fi
    mv -f -- "$part" "$path"
    CURRENT_PART=
    printf 'verified %s\n' "$name"
done
