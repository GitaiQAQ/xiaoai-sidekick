#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
while (($#)); do
    case "$1" in
        -ProjectDir|--project-dir) (($# >= 2)) || { echo "missing value for $1" >&2; exit 2; }; PROJECT_DIR=$2; shift 2 ;;
        -h|--help) echo 'Usage: Build-WakeProbes.sh [-ProjectDir DIR]'; exit 0 ;;
        *) printf 'unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done
PROJECT_DIR=$(cd -- "$PROJECT_DIR" && pwd)
source "$SCRIPT_DIR/Resolve-SystemTool.sh"
ZIG=$(resolve_system_tool zig.exe)
OUT=$PROJECT_DIR/build
mkdir -p "$OUT"
"$ZIG" cc -target arm-linux-musleabihf -mcpu=cortex_a53 -std=c11 -Wall -Wextra -Werror -Oz -static -s "$PROJECT_DIR/src/native_wake_config_probe.c" -o "$OUT/lx06-wake-config-probe-armhf"
common=(cc -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53)
"$ZIG" "${common[@]}" -c "$PROJECT_DIR/src/arm_glibc_entry.S" -o "$OUT/wake-probe-entry.o"
"$ZIG" "${common[@]}" -std=c11 -Oz -fno-stack-protector -c "$PROJECT_DIR/src/arm_glibc_entry.c" -o "$OUT/wake-probe-bridge.o"
"$ZIG" "${common[@]}" -std=c11 -Wall -Wextra -Werror -Oz -fno-stack-protector -c "$PROJECT_DIR/src/native_wake_engine_probe.c" -o "$OUT/native-wake-engine-probe.o"
"$ZIG" build-exe "$OUT/wake-probe-entry.o" "$OUT/wake-probe-bridge.o" "$OUT/native-wake-engine-probe.o" "$PROJECT_DIR/temp/rootfs-factory-system0/lib/libc-2.25.so" "$PROJECT_DIR/temp/rootfs-factory-system0/lib/libdl-2.25.so" -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -dynamic -fno-compiler-rt -fentry=_start "-femit-bin=$OUT/lx06-wake-engine-probe-armhf"
if command -v sha256sum >/dev/null 2>&1; then sha256sum "$OUT/lx06-wake-config-probe-armhf" "$OUT/lx06-wake-engine-probe-armhf"; else shasum -a 256 "$OUT/lx06-wake-config-probe-armhf" "$OUT/lx06-wake-engine-probe-armhf"; fi
