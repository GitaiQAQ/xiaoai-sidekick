#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd -- "$script_dir/.." && pwd)
defines=''
tag=run
wav=private/inputs/recordings/doubao_enroll.wav
usage() { echo 'Usage: kws-tune.sh [--defines "CFLAGS"] [--tag TAG] [--wav PATH]'; }
while (($#)); do
  case "$1" in
    --defines|-Defines) defines=${2-}; shift 2 ;;
    --tag|-Tag) tag=${2-}; shift 2 ;;
    --wav|-Wav) wav=${2-}; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done
[[ -n $tag && $tag != */* && $tag != *\\* ]] || { echo 'Tag must be a non-empty filename component' >&2; exit 2; }
cd -- "$project_dir"
zig=${LX06_ZIG:-zig}
python=${LX06_PYTHON:-}
if [[ -z $python ]]; then for c in python3 python; do if command -v "$c" >/dev/null 2>&1; then python=$c; break; fi; done; fi
[[ -n $python ]] || { echo 'python3 or python is required' >&2; exit 1; }
command -v "$zig" >/dev/null 2>&1 || { echo "Zig compiler not found: $zig" >&2; exit 1; }
[[ -f $wav ]] || { echo "WAV file not found: $wav" >&2; exit 1; }
mkdir -p build
flags=(-std=c11 -Wall -Wextra -Werror -O2 -ffp-contract=off -Isrc)
if [[ -n $defines ]]; then read -r -a define_flags <<< "$defines"; flags+=("${define_flags[@]}"); fi
exe=build/host-kws

"$zig" cc "${flags[@]}" -o "$exe" build/host_kws_main.c || { echo 'build failed' >&2; exit 1; }
"$exe" --dump "$wav" > "build/dump-$tag.txt"
"$python" tools/kws-build-templates.py "build/dump-$tag.txt" >/dev/null
"$zig" cc "${flags[@]}" -o "$exe" build/host_kws_main.c || { echo 'rebuild failed' >&2; exit 1; }
"$exe" "$wav" > "build/score-$tag.txt"
echo "== $tag ($defines)"
grep -E '^(cluster|verdict|scan_result)' "build/score-$tag.txt" || true
