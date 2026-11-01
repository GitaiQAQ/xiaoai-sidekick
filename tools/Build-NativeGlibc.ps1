param([string]$ProjectDir='C:\Users\gitai\Desktop\lx06-agent',[switch]$Ark,[switch]$Afe,[switch]$Duplex,[switch]$Kws,[switch]$Music,[switch]$Web)
$ErrorActionPreference='Continue'
$source=if($Web){'native_web_console.c'}elseif($Music){'native_music_client.c'}elseif($Ark){'native_ark_client.c'}elseif($Afe){'native_afe_frame_test.c'}elseif($Duplex){'native_duplex_client.c'}elseif($Kws){'native_kws_enroll.c'}else{'native_library_probe.c'}
$binary=if($Web){'lx06-web-armhf'}elseif($Music){'lx06-music-armhf'}elseif($Ark){'lx06-ark-client-armhf'}elseif($Afe){'lx06-afe-frame-armhf'}elseif($Duplex){'lx06-duplex-armhf'}elseif($Kws){'lx06-kws-enroll-armhf'}else{'lx06-library-probe-armhf'}
$z=Join-Path $ProjectDir 'vendor\zig-portable\zig-windows-x86_64-0.13.0\zig.exe'
$out=Join-Path $ProjectDir 'build'
[void][IO.Directory]::CreateDirectory($out)
$common=@('cc','-target','arm-linux-gnueabihf.2.25','-mcpu=cortex_a53')
& $z @common -c (Join-Path $ProjectDir 'src\arm_glibc_entry.S') -o (Join-Path $out 'arm-glibc-entry.o')
if($LASTEXITCODE -ne 0){throw 'Entry assembly compile failed'}
& $z @common -std=c11 -Oz -fno-stack-protector -c (Join-Path $ProjectDir 'src\arm_glibc_entry.c') -o (Join-Path $out 'arm-glibc-bridge.o')
if($LASTEXITCODE -ne 0){throw 'Entry bridge compile failed'}
$objName=if($Web){'native-web.o'}elseif($Music){'native-music.o'}elseif($Duplex){'native-duplex-client.o'}elseif($Kws){'native-kws-enroll.o'}else{'native-library-probe.o'}
& $z @common -std=c11 -Wall -Wextra -Werror -Oz -ffp-contract=off -I (Join-Path $ProjectDir 'vendor\curl-7.55.1-headers') -c (Join-Path $ProjectDir ('src\'+$source)) -o (Join-Path $out $objName)
if($LASTEXITCODE -ne 0){throw 'Library probe compile failed'}
$libs=@((Join-Path $out 'arm-glibc-entry.o'),(Join-Path $out 'arm-glibc-bridge.o'),(Join-Path $out $objName),
    (Join-Path $ProjectDir 'temp\rootfs-factory-system0\lib\libc-2.25.so'),
    (Join-Path $ProjectDir 'temp\rootfs-factory-system0\lib\libdl-2.25.so'))
if($Duplex){$libs+=(Join-Path $ProjectDir 'temp\rootfs-factory-system0\lib\libpthread-2.25.so')}
$link=@('build-exe')+$libs+@('-target','arm-linux-gnueabihf.2.25','-mcpu=cortex_a53','-dynamic',
    '--dynamic-linker','/lib/ld-linux-armhf.so.3',$(if($Web){'-fcompiler-rt'}else{'-fno-compiler-rt'}),'-fentry=_start',
    ('-femit-bin='+(Join-Path $out $binary)))
& $z @link
if($LASTEXITCODE -ne 0){throw 'Link against original firmware glibc failed'}
& (Join-Path $ProjectDir 'vendor\python-analysis\python.exe') (Join-Path $ProjectDir 'tools\native-library-elf.py') $binary
if($LASTEXITCODE -ne 0){throw 'Firmware-loader ELF validation failed'}
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $out $binary) | Format-List
