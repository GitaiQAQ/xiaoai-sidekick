param([string]$ProjectDir='C:\Users\gitai\Desktop\lx06-agent',
      [switch]$Start,[switch]$Stop,[switch]$Status,[switch]$ForceUpload,[switch]$FakeKey,
      [ValidateRange(15,600)][int]$Standby=60)
$ErrorActionPreference='Stop'
$dir='/data/lx06-agent'
$env:SSH_ASKPASS=Join-Path $ProjectDir 'tools\local\ssh-askpass.cmd'
$env:SSH_ASKPASS_REQUIRE='force'
$env:DISPLAY='codex'
$sshArgs='-T -o HostKeyAlgorithms=+ssh-rsa -o UserKnownHostsFile=C:/Users/gitai/Desktop/lx06-agent/tools/local/known_hosts -o ConnectTimeout=8 -o NumberOfPasswordPrompts=1 root@192.168.100.210'

function Invoke-Script([string]$script){
    $code=Get-Content -Raw (Join-Path $ProjectDir 'tools\Invoke-LX06.ps1')
    $runner=[scriptblock]::Create($code.Replace('$projectDir=Split-Path -Parent $PSScriptRoot',"`$projectDir='$ProjectDir'"))
    & $runner -Command $script
}
function Invoke-Remote([string]$remoteCommand,[string]$stdinText){
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName='C:\Windows\System32\OpenSSH\ssh.exe'
    $info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.RedirectStandardInput=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $info.Arguments="$sshArgs `"$remoteCommand`""
    $p=New-Object Diagnostics.Process;$p.StartInfo=$info
    [void]$p.Start()
    $out=$p.StandardOutput.ReadToEndAsync();$err=$p.StandardError.ReadToEndAsync()
    if($stdinText){$p.StandardInput.Write($stdinText)}
    $p.StandardInput.Close()
    if(-not $p.WaitForExit(30000)){$p.Kill();throw 'remote command timed out'}
    $o=$out.Result
    if($p.ExitCode -ne 0){throw ("remote command failed exit={0} stderr={1}" -f $p.ExitCode,$err.Result)}
    return $o
}

if($Status -or $Stop){
    if($Stop){Write-Output (Invoke-Remote "exec $dir/stop.sh" $null)}
    if($Status){Write-Output (Invoke-Remote "exec $dir/status.sh" $null)}
    return
}

$files=[ordered]@{
    'lx06-duplex-armhf'=(Join-Path $ProjectDir 'build\lx06-duplex-armhf')
    'start.sh'=(Join-Path $ProjectDir 'deploy\start.sh')
    'stop.sh'=(Join-Path $ProjectDir 'deploy\stop.sh')
    'status.sh'=(Join-Path $ProjectDir 'deploy\status.sh')
}
Invoke-Script ("mkdir -p $dir`nchmod 700 $dir`n[ -d $dir ] && echo dir_ready") | Out-Null

foreach($name in $files.Keys){
    $local=$files[$name]
    $hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $local).Hash.ToLowerInvariant()
    $remote="$dir/$name"
    $current=(Invoke-Script "sha256sum '$remote' 2>/dev/null | cut -d' ' -f1").Trim()
    if($current -eq $hash -and -not $ForceUpload){
        Write-Output "unchanged $name"
        continue
    }
    $bytes=[IO.File]::ReadAllBytes($local)
    $sb=New-Object Text.StringBuilder ($bytes.Length*4)
    foreach($b in $bytes){[void]$sb.Append('\0').Append([Convert]::ToString($b,8).PadLeft(3,'0'))}
    $octal=$sb.ToString()
    $script="set -eu`numask 077`nprintf '%b' '$octal' > '$remote.tmp'`nactual=`$(sha256sum '$remote.tmp' | cut -d' ' -f1)`n[ `"`$actual`" = '$hash' ] || { rm -f '$remote.tmp'; exit 1; }`nchmod 700 '$remote.tmp'`nmv '$remote.tmp' '$remote'`necho uploaded=$name"
    Invoke-Script $script | Out-Null
    Write-Output "uploaded $name"
}

if($Start){
    if($FakeKey){$key='public-dummy-not-a-key'}
    else{
        $cred=Get-Content -LiteralPath (Join-Path $ProjectDir 'private\credentials.json') -Raw | ConvertFrom-Json
        $key=$cred.realtime.api_key
        if($key -isnot [string] -or $key -cnotmatch '^[\x21-\x7e]{1,4096}$'){throw 'realtime key missing'}
    }
    $out=Invoke-Remote ("exec $dir/start.sh $Standby") ($key+"`n")
    $key=$null;$cred=$null
    Write-Output $out
    Start-Sleep -Seconds 2
    Write-Output (Invoke-Remote "exec $dir/status.sh" $null)
}
if($Stop){
    Write-Output (Invoke-Remote "exec $dir/stop.sh" $null)
}
if($Status){
    Write-Output (Invoke-Remote "exec $dir/status.sh" $null)
}
