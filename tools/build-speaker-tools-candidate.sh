#!/bin/sh
# LOCAL ONLY: builds two application candidates; never connects to a speaker,
# runs a wake probe, trains a model, or uploads anything.
set -eu
cd "$(dirname "$0")/.."
ZIG=${LX06_ZIG:-vendor/zig/zig-macos-aarch64-0.13.0/zig}
"$ZIG" cc -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -c src/arm_glibc_entry.S -o build/capability-entry.o
"$ZIG" cc -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -std=c11 -Oz -fno-stack-protector -c src/arm_glibc_entry.c -o build/capability-bridge.o
"$ZIG" cc -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -std=c11 -Wall -Wextra -Werror -Oz -I vendor/curl-7.55.1-headers -c src/native_web_console.c -o build/native-web-capabilities.o
"$ZIG" build-exe build/capability-entry.o build/capability-bridge.o build/native-web-capabilities.o temp/rootfs-factory-system0/lib/libc-2.25.so temp/rootfs-factory-system0/lib/libdl-2.25.so -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -O ReleaseSmall -fstrip -dynamic --dynamic-linker /lib/ld-linux-armhf.so.3 -fcompiler-rt -fentry=_start -femit-bin=build/lx06-web-capability-candidate-armhf
"$ZIG" cc -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -std=c11 -Wall -Wextra -Werror -Oz -ffp-contract=off -I vendor/curl-7.55.1-headers -c src/native_duplex_client.c -o build/native-duplex-capabilities.o
"$ZIG" build-exe build/capability-entry.o build/capability-bridge.o build/native-duplex-capabilities.o temp/rootfs-factory-system0/lib/libc-2.25.so temp/rootfs-factory-system0/lib/libdl-2.25.so temp/rootfs-factory-system0/lib/libpthread-2.25.so -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -O ReleaseSmall -fstrip -dynamic --dynamic-linker /lib/ld-linux-armhf.so.3 -fno-compiler-rt -fentry=_start -femit-bin=build/lx06-duplex-capability-candidate-armhf
shasum -a 256 build/lx06-web-capability-candidate-armhf build/lx06-duplex-capability-candidate-armhf
