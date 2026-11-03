param([string]$ProjectDir='C:\Users\gitai\Desktop\lx06-agent',
      [ValidateSet('probe','socket-test','observer-test','observer','libraries','tls','ark-test','ws-transport','ws-test','ws-loopback','afe-frame','kws-enroll','feedback-test','kws-score','kws-live','kws-capture','kws-replay','kws-standby','kws-signal')][string]$Kind='probe',
      [ValidateRange(2,4194304)][int]$ThreadId=2601,
      [ValidateRange(0,1024)][int]$SocketFd=8,
      [ValidateRange(1,300)][int]$Seconds=5)
$ErrorActionPreference='Stop'
. (Join-Path $ProjectDir 'tools\Resolve-SystemTool.ps1')
$python=Resolve-SystemTool 'python.exe'
$name=switch($Kind){'probe'{'lx06-probe-armhf'} 'socket-test'{'lx06-socket-test-armhf'} 'libraries'{'lx06-library-probe-armhf'} 'tls'{'lx06-library-probe-armhf'} 'ark-test'{'lx06-ark-client-armhf'} 'ws-transport'{'lx06-library-probe-armhf'} 'ws-test'{'lx06-library-probe-armhf'} 'ws-loopback'{'lx06-library-probe-armhf'} 'afe-frame'{'lx06-afe-frame-armhf'} 'kws-enroll'{'lx06-kws-enroll-armhf'} 'feedback-test'{'lx06-duplex-armhf'} 'kws-score'{'lx06-duplex-armhf'} 'kws-live'{'lx06-duplex-armhf'} 'kws-capture'{'lx06-duplex-armhf'} 'kws-replay'{'lx06-duplex-armhf'} 'kws-standby'{'lx06-duplex-armhf'} 'kws-signal'{'lx06-duplex-armhf'} default{'lx06-speech-observer-armhf'}}
$file=Join-Path $ProjectDir "build\$name"
if($Kind -in @('libraries','tls','ark-test','ws-transport','ws-test','ws-loopback','afe-frame','kws-enroll','feedback-test','kws-score','kws-live','kws-capture','kws-replay','kws-standby','kws-signal')){
    & $python (Join-Path $ProjectDir 'tools\native-library-elf.py') $name
    if($LASTEXITCODE -ne 0){throw 'Firmware-loader ELF validation failed'}
}
$hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $file).Hash.ToLowerInvariant()
$remote="/tmp/lx06-agent-$Kind-$($hash.Substring(0,12))"
$octal=-join ([IO.File]::ReadAllBytes($file) | ForEach-Object {'\0'+[Convert]::ToString($_,8).PadLeft(3,'0')})
$template=@'
set -eu
dst='__REMOTE__'
if [ -e "$dst" ] || [ -e "$dst.trace" ]; then echo 'refusing to overwrite existing probe path' >&2; exit 1; fi
umask 077
trap 'rm -f "$dst" "$dst.trace"' EXIT
echo 'native_processes_before'
pidof mipns-xiaomi
pidof mico_aivs_lab
printf '%b' '__OCTAL__' > "$dst"
actual=$(sha256sum "$dst")
actual=${actual%% *}
if [ "$actual" != '__HASH__' ]; then echo 'remote checksum mismatch' >&2; exit 1; fi
chmod 700 "$dst"
__RUN__
rm -f "$dst"
[ ! -e "$dst" ]
echo 'temporary_binary_removed=1'
echo 'native_processes_after'
pidof mipns-xiaomi
pidof mico_aivs_lab
'@
$run=switch($Kind){
    'probe'{'"$dst" --self-test' + "`n" + '"$dst" --status'}
    'socket-test'{'"$dst"'}
    'libraries'{'strace -f -s 0 -e trace=network -o "$dst.trace" /lib/ld-linux-armhf.so.3 "$dst"' + "`n" + 'cat "$dst.trace"' + "`n" + 'rm -f "$dst.trace"'}
    'ark-test'{'/lib/ld-linux-armhf.so.3 "$dst" --self-test'}
    'ws-test'{'/lib/ld-linux-armhf.so.3 "$dst" --ws-self-test'}
    'ws-transport'{'/lib/ld-linux-armhf.so.3 "$dst" --ws-probe'}
    'afe-frame'{'/lib/ld-linux-armhf.so.3 "$dst"'}
    'kws-enroll'{'/lib/ld-linux-armhf.so.3 "$dst" /tmp/doubao_enroll.wav'}
    'feedback-test'{'timeout -t 12 /lib/ld-linux-armhf.so.3 "$dst" --feedback-test'}
    'kws-score'{'/lib/ld-linux-armhf.so.3 "$dst" --kws-score /tmp/doubao_enroll.wav'}
    'kws-live'{'timeout -t '+($Seconds+8)+' /lib/ld-linux-armhf.so.3 "$dst" --kws-live '+$Seconds}
    'kws-capture'{'timeout -t '+($Seconds+8)+' /lib/ld-linux-armhf.so.3 "$dst" --kws-capture '+$Seconds}
    'kws-replay'{'/lib/ld-linux-armhf.so.3 "$dst" --kws-wav /tmp/doubao_enroll.wav < /dev/null'}
    'kws-signal'{'if timeout -t '+$Seconds+' /lib/ld-linux-armhf.so.3 "$dst" --kws 300 < /dev/null; then echo "signal_exit=0"; else echo "signal_exit=$?"; fi'}
    'kws-standby'{'if timeout -t '+($Seconds+10)+' /lib/ld-linux-armhf.so.3 "$dst" --kws '+$Seconds+' < /dev/null; then echo "standby_exit=0"; else echo "standby_exit=$?"; fi'}
    'ws-loopback'{@'
LD=/lib/ld-linux-armhf.so.3
run_case() {
  scenario="$1"
  "$LD" "$dst" --ws-test-server 18099 "$scenario" > "/tmp/lx06-ws-srv-$scenario.log" 2>&1 &
  srv=$!
  sleep 1
  set +e
  "$LD" "$dst" --ws-local 18099
  rc=$?
  wait $srv
  src=$?
  set -e
  echo "CASE $scenario client_rc=$rc server_rc=$src"
  cat "/tmp/lx06-ws-srv-$scenario.log"
  rm -f "/tmp/lx06-ws-srv-$scenario.log"
}
run_case good
run_case badaccept
run_case status400
run_case silent
'@}
    'tls'{'/lib/ld-linux-armhf.so.3 "$dst" --tls'}
    'observer-test'{'"$dst" --self-test'}
    'observer'{'"$dst" --observe '+$ThreadId+' '+$SocketFd+' '+$Seconds}
}
$command=$template.Replace('__REMOTE__',$remote).Replace('__OCTAL__',$octal).Replace('__HASH__',$hash).Replace('__RUN__',$run)
$code=Get-Content -Raw -LiteralPath (Join-Path $ProjectDir 'tools\Invoke-LX06.ps1')
$runner=[scriptblock]::Create($code.Replace('$projectDir=Split-Path -Parent $PSScriptRoot',"`$projectDir='$ProjectDir'"))
& $runner -Command $command -OutputFile (Join-Path $ProjectDir "logs\native-$Kind-runtime.txt")
Get-Content -LiteralPath (Join-Path $ProjectDir "logs\native-$Kind-runtime.txt")



