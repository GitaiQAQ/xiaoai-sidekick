param([string]$ProjectDir='C:\Users\gitai\Desktop\lx06-agent',[switch]$Install,[switch]$Start,[switch]$Stop,[switch]$Status,[switch]$NoVoiceKey)
$ErrorActionPreference='Stop'
. (Join-Path $ProjectDir 'tools\Resolve-SystemTool.ps1')
$python=Resolve-SystemTool 'python.exe'
$dir='/data/lx06-agent'
$code=Get-Content -Raw (Join-Path $ProjectDir 'tools\Invoke-LX06.ps1')
$runner=[scriptblock]::Create($code.Replace('$projectDir=Split-Path -Parent $PSScriptRoot',"`$projectDir='$ProjectDir'"))
function Remote([string]$text){ & $runner -Command $text }
if($Stop -or $Install){
 $stopScript=@'
set -eu
f=/data/lx06-agent/web.pid
if [ -f "$f" ]; then
 p=$(cat "$f")
 case "$p" in ''|*[!0-9]*) echo invalid_pid >&2; exit 1;; esac
 if [ -r "/proc/$p/cmdline" ] && tr '\000' '\n' < "/proc/$p/cmdline" | grep -Fxq '/data/lx06-agent/lx06-web-armhf'; then
  kill "$p"
  n=0
  while kill -0 "$p" 2>/dev/null && [ "$n" -lt 12 ]; do sleep 1; n=$((n+1)); done
  if kill -0 "$p" 2>/dev/null; then echo web_stop_pending; exit 1; fi
 fi
 rm -f "$f"
fi
echo web_stopped
'@
 Remote $stopScript
}
if($Install){
 & $python (Join-Path $ProjectDir 'tools\native-library-elf.py') lx06-web-armhf
 if($LASTEXITCODE){throw 'ELF validation failed'}
 Remote 'set -eu; mkdir -p /data/lx06-agent/web; chmod 700 /data/lx06-agent /data/lx06-agent/web'
 $files=[ordered]@{
  'lx06-web-armhf'='build\lx06-web-armhf'
  'lx06-duplex-armhf'='build\lx06-duplex-armhf'
  'web/index.html'='web\index.html'
  'web/app.css'='web\app.css'
  'web/app.js'='web\app.js'
  'web/qrcode.js'='web\qrcode.js'
  'web/QRCODE-LICENSE.txt'='web\QRCODE-LICENSE.txt'
 }
 foreach($name in $files.Keys){
  $path=Join-Path $ProjectDir $files[$name]
  $hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
  $dst="$dir/$name"
  $existing=(Remote "sha256sum '$dst' 2>/dev/null | cut -d' ' -f1").Trim()
  if($existing -eq $hash){Write-Output "unchanged $name";continue}
  if($name -eq 'lx06-duplex-armhf'){
   $voiceCheck=@'
for f in /proc/[0-9]*/cmdline; do
 if tr '\000' '\n' < "$f" 2>/dev/null | grep -Fxq '/data/lx06-agent/lx06-duplex-armhf'; then echo voice_running; break; fi
done
'@
   if((Remote $voiceCheck).Trim()){throw 'Stop voice assistant before updating its binary'}
  }
  $oct=-join([IO.File]::ReadAllBytes($path)|ForEach-Object{'\0'+[Convert]::ToString($_,8).PadLeft(3,'0')})
  $mode=if($name -like '*armhf'){'700'}else{'600'}
  Remote "set -eu`numask 077`nprintf '%b' '$oct' > '$dst.new'`n[ `"`$(sha256sum '$dst.new' | cut -d' ' -f1)`" = '$hash' ] || { rm -f '$dst.new'; exit 1; }`nchmod $mode '$dst.new'`nmv '$dst.new' '$dst'`necho installed=$name"
 }
}
if($Start){
 $key=''
 if(-not $NoVoiceKey){
  $path=Join-Path $ProjectDir 'private\credentials.json'
  if(Test-Path -LiteralPath $path){
   $config=[IO.File]::ReadAllText($path)|ConvertFrom-Json
   if($config.realtime.api_key -is [string] -and $config.realtime.api_key -cmatch '^[\x21-\x7e]{1,4096}$'){$key=$config.realtime.api_key}
   $config=$null
  }
 }
 $info=New-Object Diagnostics.ProcessStartInfo
 $info.FileName='C:\Windows\System32\OpenSSH\ssh.exe'
 $env:SSH_ASKPASS=Join-Path $ProjectDir 'tools\local\ssh-askpass.cmd'
 $env:SSH_ASKPASS_REQUIRE='force'
 $env:DISPLAY='codex'
 $info.UseShellExecute=$false;$info.CreateNoWindow=$true
 $info.RedirectStandardInput=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $info.Arguments='-T -o HostKeyAlgorithms=+ssh-rsa -o UserKnownHostsFile=C:/Users/gitai/Desktop/lx06-agent/tools/local/known_hosts -o ConnectTimeout=8 -o NumberOfPasswordPrompts=1 root@192.168.100.210 "exec /lib/ld-linux-armhf.so.3 /data/lx06-agent/lx06-web-armhf --daemon > /data/lx06-agent/web.log 2>&1"'
 $p=New-Object Diagnostics.Process;$p.StartInfo=$info;[void]$p.Start()
 $output=$p.StandardOutput.ReadToEndAsync();$errors=$p.StandardError.ReadToEndAsync()
 $p.StandardInput.Write($key+"`n");$p.StandardInput.Close();$key=$null
 if(-not $p.WaitForExit(15000)){$p.Kill();throw 'Web start SSH timed out'}
 if($p.ExitCode){throw 'Web service failed to start; inspect metadata log'}
 Write-Output 'web_start_requested'
}
if($Status -or $Start){Remote 'cat /data/lx06-agent/web.pid 2>/dev/null || true; tail -n 2 /data/lx06-agent/web.log 2>/dev/null || true'}
