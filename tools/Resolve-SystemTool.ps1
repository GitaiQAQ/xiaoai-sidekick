function Resolve-SystemTool {
    param([Parameter(Mandatory)][string]$Name)

    $pathSources = @(
        [Environment]::GetEnvironmentVariable('Path', 'User'),
        [Environment]::GetEnvironmentVariable('Path', 'Machine'),
        $env:Path
    )
    foreach ($entry in ($pathSources -join ';' -split ';' | Select-Object -Unique)) {
        if ([string]::IsNullOrWhiteSpace($entry)) { continue }
        if ($Name -match '^python(?:3)?\.exe$' -and $entry.TrimEnd('\') -match '(?i)\\Microsoft\\WindowsApps$') { continue }
        $candidate = Join-Path $entry $Name
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }

    $command = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command -and $command.Source) { return $command.Source }
    throw "Required system tool '$Name' was not found on PATH."
}
