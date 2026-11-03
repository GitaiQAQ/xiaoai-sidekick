#!/bin/sh
# macOS ARM/glibc probe; run on LX06 via /lib/ld-linux-armhf.so.3.
set -eu
cd "$(dirname "$0")/.."
ZIG=${LX06_ZIG:-vendor/zig/zig-macos-aarch64-0.13.0/zig}
mkdir -p build
"$ZIG" cc -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -c src/arm_glibc_entry.S -o build/wake-probe-entry.o
"$ZIG" cc -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -std=c11 -Oz -fno-stack-protector -c src/arm_glibc_entry.c -o build/wake-probe-bridge.o
"$ZIG" cc -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -std=c11 -Wall -Wextra -Werror -Oz -fno-stack-protector -c src/native_wake_model_probe.c -o build/native-wake-model-probe.o
"$ZIG" build-exe build/wake-probe-entry.o build/wake-probe-bridge.o build/native-wake-model-probe.o temp/rootfs-factory-system0/lib/libc-2.25.so temp/rootfs-factory-system0/lib/libdl-2.25.so -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -dynamic --dynamic-linker /lib/ld-linux-armhf.so.3 -fno-compiler-rt -fentry=_start -femit-bin=build/lx06-wake-model-probe-armhf
shasum -a 256 build/lx06-wake-model-probe-armhf
