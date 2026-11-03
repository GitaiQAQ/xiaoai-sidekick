param([string]$Defines='',[string]$Tag='run',[string]$Wav='private\inputs\recordings\doubao_enroll.wav')
# Host-side tuning loop: build scorer -> dump -> regenerate templates -> rescore.
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
Set-Location $project
. (Join-Path $project 'tools\Resolve-SystemTool.ps1')
$z=Resolve-SystemTool 'zig.exe'
$python=Resolve-SystemTool 'python.exe'
$flags=@('-target','x86_64-windows-gnu','-std=c11','-Wall','-Wextra','-Werror','-O2','-ffp-contract=off','-Isrc')
if($Defines){$flags+=($Defines -split ' ')}
[void][IO.Directory]::CreateDirectory((Join-Path $project 'build'))
& $z cc @flags -o build\host-kws.exe build\host_kws_main.c
if($LASTEXITCODE -ne 0){throw 'build failed'}
& .\build\host-kws.exe --dump $Wav | Out-File -Encoding utf8 "build\dump-$Tag.txt"
& $python tools\kws-build-templates.py "build\dump-$Tag.txt" | Out-Null
& $z cc @flags -o build\host-kws.exe build\host_kws_main.c
if($LASTEXITCODE -ne 0){throw 'rebuild failed'}
& .\build\host-kws.exe $Wav | Out-File -Encoding utf8 "build\score-$Tag.txt"
Write-Output "== $Tag ($Defines)"
Select-String -Path "build\score-$Tag.txt" -Pattern '^(cluster|verdict|scan_result)' | ForEach-Object { $_.Line }
