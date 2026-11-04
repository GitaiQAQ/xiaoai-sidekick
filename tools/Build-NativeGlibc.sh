#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
ARK=0 AFE=0 DUPLEX=0 KWS=0 MUSIC=0 WEB=0
while (($#)); do
    case "$1" in
        -ProjectDir|--project-dir) (($# >= 2)) || { echo "missing value for $1" >&2; exit 2; }; PROJECT_DIR=$2; shift 2 ;;
        -Ark|--ark) ARK=1; shift ;;
        -Afe|--afe) AFE=1; shift ;;
        -Duplex|--duplex) DUPLEX=1; shift ;;
        -Kws|--kws) KWS=1; shift ;;
        -Music|--music) MUSIC=1; shift ;;
        -Web|--web) WEB=1; shift ;;
        -h|--help) echo 'Usage: Build-NativeGlibc.sh [-ProjectDir DIR] [-Ark|-Afe|-Duplex|-Kws|-Music|-Web]'; exit 0 ;;
        *) printf 'unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done
PROJECT_DIR=$(cd -- "$PROJECT_DIR" && pwd)
source "$SCRIPT_DIR/Resolve-SystemTool.sh"
ZIG=$(resolve_system_tool zig)
PYTHON=$(resolve_system_tool python3)

if ((WEB)); then source_name=native_web_console.c; binary=lx06-web-armhf
elif ((MUSIC)); then source_name=native_music_client.c; binary=lx06-music-armhf
elif ((ARK)); then source_name=native_ark_client.c; binary=lx06-ark-client-armhf
elif ((AFE)); then source_name=native_afe_frame_test.c; binary=lx06-afe-frame-armhf
elif ((DUPLEX)); then source_name=native_duplex_client.c; binary=lx06-duplex-armhf
elif ((KWS)); then source_name=native_kws_enroll.c; binary=lx06-kws-enroll-armhf
else source_name=native_library_probe.c; binary=lx06-library-probe-armhf
fi
case "$binary" in
    lx06-web-armhf) obj_name=native-web.o ;;
    lx06-music-armhf) obj_name=native-music.o ;;
    lx06-duplex-armhf) obj_name=native-duplex-client.o ;;
    lx06-kws-enroll-armhf) obj_name=native-kws-enroll.o ;;
    *) obj_name=native-library-probe.o ;;
esac

OUT=$PROJECT_DIR/build
mkdir -p "$OUT"
common=(cc -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53)
"$ZIG" "${common[@]}" -c "$PROJECT_DIR/src/arm_glibc_entry.S" -o "$OUT/arm-glibc-entry.o"
"$ZIG" "${common[@]}" -std=c11 -Oz -fno-stack-protector -c "$PROJECT_DIR/src/arm_glibc_entry.c" -o "$OUT/arm-glibc-bridge.o"
"$ZIG" "${common[@]}" -std=c11 -Wall -Wextra -Werror -Oz -ffp-contract=off -I "$PROJECT_DIR/third_party/curl-7.55.1/include" -c "$PROJECT_DIR/src/$source_name" -o "$OUT/$obj_name"

libs=("$OUT/arm-glibc-entry.o" "$OUT/arm-glibc-bridge.o" "$OUT/$obj_name" "$PROJECT_DIR/temp/rootfs-factory-system0/lib/libc-2.25.so" "$PROJECT_DIR/temp/rootfs-factory-system0/lib/libdl-2.25.so")
if ((DUPLEX)); then libs+=("$PROJECT_DIR/temp/rootfs-factory-system0/lib/libpthread-2.25.so"); fi
compiler_rt=-fno-compiler-rt
if ((WEB)); then compiler_rt=-fcompiler-rt; fi
"$ZIG" build-exe "${libs[@]}" -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -dynamic --dynamic-linker /lib/ld-linux-armhf.so.3 "$compiler_rt" -fentry=_start "-femit-bin=$OUT/$binary"
"$PYTHON" "$PROJECT_DIR/tools/native-library-elf.py" "$binary"
if command -v sha256sum >/dev/null 2>&1; then sha256sum "$OUT/$binary"; else shasum -a 256 "$OUT/$binary"; fi
