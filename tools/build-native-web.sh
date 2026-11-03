#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
ZIG=${LX06_ZIG:-zig}
"$ZIG" cc -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -c src/arm_glibc_entry.S -o build/wake-probe-entry.o
"$ZIG" cc -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -std=c11 -Oz -fno-stack-protector -c src/arm_glibc_entry.c -o build/wake-probe-bridge.o
"$ZIG" cc -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -std=c11 -Wall -Wextra -Werror -Oz -I third_party/curl-7.55.1/include -c src/native_web_console.c -o build/native-web.o
"$ZIG" build-exe build/wake-probe-entry.o build/wake-probe-bridge.o build/native-web.o temp/rootfs-factory-system0/lib/libc-2.25.so temp/rootfs-factory-system0/lib/libdl-2.25.so -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -dynamic --dynamic-linker /lib/ld-linux-armhf.so.3 -fcompiler-rt -fentry=_start -femit-bin=build/lx06-web-armhf
shasum -a 256 build/lx06-web-armhf
