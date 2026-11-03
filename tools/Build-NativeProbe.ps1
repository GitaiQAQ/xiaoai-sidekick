param([string]$ProjectDir='C:\Users\gitai\Desktop\lx06-agent',[switch]$HostTest)
$ErrorActionPreference='Stop'
. (Join-Path $ProjectDir 'tools\Resolve-SystemTool.ps1')
$zig=Resolve-SystemTool 'zig.exe'
$out=Join-Path $ProjectDir 'build'
[void][IO.Directory]::CreateDirectory($out)
$sources=@((Join-Path $ProjectDir 'src\native_protocol.c'),(Join-Path $ProjectDir 'src\lx06_probe.c'))
# Native compiler warnings on stderr must not become terminating PowerShell errors.
$ErrorActionPreference='Continue'
if($HostTest){
    & $zig cc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -O2 @sources -o (Join-Path $out 'lx06-probe-host.exe')
    if($LASTEXITCODE -ne 0){throw 'Host compile failed'}
    & (Join-Path $out 'lx06-probe-host.exe') --self-test
    if($LASTEXITCODE -ne 0){throw 'Host protocol self-test failed'}
}
& $zig cc -target arm-linux-musleabihf -mcpu=cortex_a53 -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -Oz -static -s @sources -o (Join-Path $out 'lx06-probe-armhf')
if($LASTEXITCODE -ne 0){throw 'ARM compile failed'}
& $zig cc -target arm-linux-musleabihf -mcpu=cortex_a53 -std=c11 -Wall -Wextra -Werror -Oz -static -s (Join-Path $ProjectDir 'src\unix_handover_test.c') -o (Join-Path $out 'lx06-socket-test-armhf')
if($LASTEXITCODE -ne 0){throw 'ARM socket test compile failed'}
& $zig cc -target arm-linux-musleabihf -mcpu=cortex_a53 -std=c11 -Wall -Wextra -Werror -Oz -static -s (Join-Path $ProjectDir 'src\native_protocol.c') (Join-Path $ProjectDir 'src\native_speech_observer.c') -o (Join-Path $out 'lx06-speech-observer-armhf')
if($LASTEXITCODE -ne 0){throw 'ARM speech observer compile failed'}
& ([scriptblock]::Create((Get-Content -Raw -LiteralPath (Join-Path $ProjectDir 'tools\Build-NativeGlibc.ps1')))) -ProjectDir $ProjectDir
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $out 'lx06-probe-armhf') | Format-List
