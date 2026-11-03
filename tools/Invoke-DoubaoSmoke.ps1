param([string]$ProjectDir='C:\Users\gitai\Desktop\lx06-agent',
      [string]$CredentialsFile='', [switch]$TransportTest,[switch]$Realtime,[switch]$Session,[switch]$Responses,[switch]$Duplex,[switch]$Kws,[switch]$KwsWav,[switch]$Tone,[switch]$MakeAck,[int]$Seconds=30,[int]$KwsSeconds=60)
$ErrorActionPreference='Stop'
. (Join-Path $ProjectDir 'tools\Resolve-SystemTool.ps1')
$python=Resolve-SystemTool 'python.exe'
$binary=if($Duplex){'lx06-duplex-armhf'}elseif($Realtime){'lx06-library-probe-armhf'}else{'lx06-ark-client-armhf'}
$service=if($Realtime -or $Duplex){'realtime'}else{'ark'}
if($TransportTest){
    $payload=if($Realtime){'public-dummy-not-a-key'}else{'{"api_key":"public-dummy-not-a-key","model":"ep-test"}'}
    $mode=if($Realtime){'--ws-check-key'}else{'--check-config'}
}else{
    if(-not $CredentialsFile){$CredentialsFile=Join-Path $ProjectDir 'private\credentials.json'}
    if((Get-Item -LiteralPath $CredentialsFile).Length -gt 65536){throw 'Credential file exceeds size limit'}
    try{$config=[IO.File]::ReadAllText($CredentialsFile) | ConvertFrom-Json}catch{throw 'Invalid credential JSON'}
    $section=$config.$service
    if($config.schema_version -ne 1 -or $section.api_key -isnot [string] -or
       $section.api_key -cnotmatch '^[\x21-\x7e]{1,4096}$'){
        throw 'Service key missing or invalid; values suppressed'
    }
    if($Realtime -or $Duplex){$payload=$section.api_key;$mode=if($Tone){'--tone'}elseif($MakeAck){'--make-ack /data/lx06-agent/wake_ack.wav'}elseif($Kws){"--kws $KwsSeconds"}elseif($KwsWav){'--kws-wav /tmp/doubao_enroll.wav'}elseif($Duplex){"$Seconds"}elseif($Session){'--ws-session'}else{'--ws-stdin'}}
    else{
        if($section.model -isnot [string] -or $section.model -cnotmatch '^[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}$'){
            throw 'Ark model missing or invalid; value suppressed'
        }
        $payload=@{api_key=$section.api_key;model=$section.model}|ConvertTo-Json -Compress
        $mode=if($Responses){'--responses'}else{'--stdin-config'}
    }
}
& $python (Join-Path $ProjectDir 'tools\native-library-elf.py') $binary
if($LASTEXITCODE -ne 0){throw 'ELF validation failed'}
$file=Join-Path $ProjectDir ('build\'+$binary)
$hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $file).Hash.ToLowerInvariant()
$remote='/tmp/lx06-agent-'+$service+'-'+[Guid]::NewGuid().ToString('N')
$octal=-join ([IO.File]::ReadAllBytes($file)|ForEach-Object{'\0'+[Convert]::ToString($_,8).PadLeft(3,'0')})
$code=Get-Content -Raw -LiteralPath (Join-Path $ProjectDir 'tools\Invoke-LX06.ps1')
$runner=[scriptblock]::Create($code.Replace('$projectDir=Split-Path -Parent $PSScriptRoot',"`$projectDir='$ProjectDir'"))
$upload=@'
set -eu
dst='__PATH__'
[ ! -e "$dst" ]
umask 077
printf '%b' '__OCTAL__' > "$dst"
actual=$(sha256sum "$dst")
actual=${actual%% *}
if [ "$actual" != '__HASH__' ]; then rm -f "$dst"; exit 1; fi
chmod 700 "$dst"
echo 'doubao_binary_sha_verified=1'
'@
try{
    & $runner -Command $upload.Replace('__PATH__',$remote).Replace('__OCTAL__',$octal).Replace('__HASH__',$hash)
    # Fixed remote command; only filtered credentials travel over stdin, never through shell/argv.
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName='C:\Windows\System32\OpenSSH\ssh.exe'
    $info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.RedirectStandardInput=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $info.Arguments='-T -o HostKeyAlgorithms=+ssh-rsa -o UserKnownHostsFile=C:/Users/gitai/Desktop/lx06-agent/tools/local/known_hosts -o ConnectTimeout=8 -o NumberOfPasswordPrompts=1 root@192.168.100.210 "exec /lib/ld-linux-armhf.so.3 '+$remote+' '+$mode+'"'
    $process=New-Object Diagnostics.Process;$process.StartInfo=$info
    [void]$process.Start()
    $readout=$process.StandardOutput.ReadToEndAsync();$readerr=$process.StandardError.ReadToEndAsync()
    $process.StandardInput.Write($payload);$process.StandardInput.Close()
    $limit=if($Duplex){([Math]::Max($Seconds,$KwsSeconds)+60)*1000}else{35000}
    if(-not $process.WaitForExit($limit)){$process.Kill();throw 'Doubao SSH operation timed out'}
    $output=$readout.Result
    # Native program emits metadata only. Do not include SSH stderr in errors.
    $log=if($TransportTest){$service+'-stdin-transport-test.txt'}elseif($Duplex -and $KwsWav){'kws-session-runtime.txt'}elseif($Duplex){'duplex-runtime.txt'}else{$service+'-auth-smoke.txt'}
    [IO.File]::WriteAllText((Join-Path $ProjectDir ('logs\'+$log)),$output)
    Write-Output $output
    if($process.ExitCode -ne 0){throw 'Doubao operation failed; inspect metadata log (secret output suppressed)'}
}finally{
    $payload=$null;$config=$null;$section=$null
    & $runner -Command ("rm -f '$remote'`n[ ! -e '$remote' ] && echo doubao_temporary_binary_removed=1`npidof mipns-xiaomi`npidof mico_aivs_lab`nubus call mibt_mesh mesh_status '{}'`n")
}
