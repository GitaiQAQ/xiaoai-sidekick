param(
    [string]$ProjectDir = (Split-Path -Parent $PSScriptRoot),
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$base = 'https://github.com/idootop/open-xiaoai/releases/download/LX06_1.94.13'
$items = @(
    @{ Name = 'LX06_1.94.13.squashfs';         Sha = '317a54909f12b63f22128965bf9005c06435d15f61c4202bb88c4524bf1e2ef4' },
    @{ Name = 'LX06_1.94.13_patched.squashfs'; Sha = '3fbcfcd8a2130f5421d0bc3a3b22cb5a3de59b5d3678a3a14b23541aa9474225' }
)

$dest = Join-Path $ProjectDir 'rom'
New-Item -ItemType Directory -Path $dest -Force | Out-Null

foreach ($item in $items) {
    $path = Join-Path $dest $item.Name
    if ((Test-Path -LiteralPath $path) -and -not $Force) {
        $have = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLower()
        if ($have -eq $item.Sha) { Write-Output "present $($item.Name)"; continue }
        Write-Output "hash_mismatch $($item.Name) expected=$($item.Sha) actual=$have"
    }
    $url = "$base/$($item.Name)"
    Write-Output "download $url"
    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile "$path.part"
    $got = (Get-FileHash -LiteralPath "$path.part" -Algorithm SHA256).Hash.ToLower()
    if ($got -ne $item.Sha) {
        Remove-Item -LiteralPath "$path.part" -Force
        throw "sha256 mismatch for $($item.Name): expected $($item.Sha), got $got"
    }
    Move-Item -LiteralPath "$path.part" -Destination $path -Force
    Write-Output "verified $($item.Name)"
}
