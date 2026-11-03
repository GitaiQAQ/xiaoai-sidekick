#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
ZIG=${LX06_ZIG:-vendor/zig/zig-macos-aarch64-0.13.0/zig}
sh tools/build-wake-model-probe.sh
"$ZIG" cc -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -std=c11 -Wall -Wextra -Werror -Oz -ffp-contract=off -I vendor/curl-7.55.1-headers -c src/native_duplex_client.c -o build/native-duplex-client.o
"$ZIG" build-exe build/wake-probe-entry.o build/wake-probe-bridge.o build/native-duplex-client.o temp/rootfs-factory-system0/lib/libc-2.25.so temp/rootfs-factory-system0/lib/libdl-2.25.so temp/rootfs-factory-system0/lib/libpthread-2.25.so -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -dynamic --dynamic-linker /lib/ld-linux-armhf.so.3 -fno-compiler-rt -fentry=_start -femit-bin=build/lx06-duplex-factory-wake-armhf
shasum -a 256 build/lx06-duplex-factory-wake-armhf
