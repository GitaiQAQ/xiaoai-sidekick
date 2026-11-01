param([string]$ProjectDir=(Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference='Continue'
$zig=Join-Path $ProjectDir 'vendor/zig-portable/zig-windows-x86_64-0.13.0/zig.exe'
$out=Join-Path $ProjectDir 'build'
[void][IO.Directory]::CreateDirectory($out)
& $zig cc -target arm-linux-musleabihf -mcpu=cortex_a53 -std=c11 -Wall -Wextra -Werror -Oz -static -s (Join-Path $ProjectDir 'src/native_wake_config_probe.c') -o (Join-Path $out 'lx06-wake-config-probe-armhf')
if($LASTEXITCODE -ne 0){throw 'Wake config probe build failed'}
$common=@('cc','-target','arm-linux-gnueabihf.2.25','-mcpu=cortex_a53')
& $zig @common -c (Join-Path $ProjectDir 'src/arm_glibc_entry.S') -o (Join-Path $out 'wake-probe-entry.o')
if($LASTEXITCODE -ne 0){throw 'Wake probe entry build failed'}
& $zig @common -std=c11 -Oz -fno-stack-protector -c (Join-Path $ProjectDir 'src/arm_glibc_entry.c') -o (Join-Path $out 'wake-probe-bridge.o')
if($LASTEXITCODE -ne 0){throw 'Wake probe bridge build failed'}
& $zig @common -std=c11 -Wall -Wextra -Werror -Oz -fno-stack-protector -c (Join-Path $ProjectDir 'src/native_wake_engine_probe.c') -o (Join-Path $out 'native-wake-engine-probe.o')
if($LASTEXITCODE -ne 0){throw 'Wake engine probe compile failed'}
& $zig build-exe (Join-Path $out 'wake-probe-entry.o') (Join-Path $out 'wake-probe-bridge.o') (Join-Path $out 'native-wake-engine-probe.o') (Join-Path $ProjectDir 'temp/rootfs-factory-system0/lib/libc-2.25.so') (Join-Path $ProjectDir 'temp/rootfs-factory-system0/lib/libdl-2.25.so') -target arm-linux-gnueabihf.2.25 -mcpu=cortex_a53 -dynamic -fno-compiler-rt -fentry=_start ('-femit-bin='+(Join-Path $out 'lx06-wake-engine-probe-armhf'))
if($LASTEXITCODE -ne 0){throw 'Wake engine probe link failed'}
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $out 'lx06-wake-config-probe-armhf'),(Join-Path $out 'lx06-wake-engine-probe-armhf')
