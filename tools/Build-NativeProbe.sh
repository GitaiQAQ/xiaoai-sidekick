#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
HOST_TEST=0
while (($#)); do
    case "$1" in
        -ProjectDir|--project-dir) (($# >= 2)) || { echo "missing value for $1" >&2; exit 2; }; PROJECT_DIR=$2; shift 2 ;;
        -HostTest|--host-test) HOST_TEST=1; shift ;;
        -h|--help) echo 'Usage: Build-NativeProbe.sh [-ProjectDir DIR] [-HostTest]'; exit 0 ;;
        *) printf 'unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done
PROJECT_DIR=$(cd -- "$PROJECT_DIR" && pwd)
source "$SCRIPT_DIR/Resolve-SystemTool.sh"
ZIG=$(resolve_system_tool zig.exe)
OUT=$PROJECT_DIR/build
mkdir -p "$OUT"
sources=("$PROJECT_DIR/src/native_protocol.c" "$PROJECT_DIR/src/lx06_probe.c")
if ((HOST_TEST)); then
    "$ZIG" cc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -O2 "${sources[@]}" -o "$OUT/lx06-probe-host.exe"
    "$OUT/lx06-probe-host.exe" --self-test
fi
"$ZIG" cc -target arm-linux-musleabihf -mcpu=cortex_a53 -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -Oz -static -s "${sources[@]}" -o "$OUT/lx06-probe-armhf"
"$ZIG" cc -target arm-linux-musleabihf -mcpu=cortex_a53 -std=c11 -Wall -Wextra -Werror -Oz -static -s "$PROJECT_DIR/src/unix_handover_test.c" -o "$OUT/lx06-socket-test-armhf"
"$ZIG" cc -target arm-linux-musleabihf -mcpu=cortex_a53 -std=c11 -Wall -Wextra -Werror -Oz -static -s "$PROJECT_DIR/src/native_protocol.c" "$PROJECT_DIR/src/native_speech_observer.c" -o "$OUT/lx06-speech-observer-armhf"
"$SCRIPT_DIR/Build-NativeGlibc.sh" -ProjectDir "$PROJECT_DIR"
if command -v sha256sum >/dev/null 2>&1; then sha256sum "$OUT/lx06-probe-armhf"; else shasum -a 256 "$OUT/lx06-probe-armhf"; fi
