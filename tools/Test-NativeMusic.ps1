param([string]$ProjectDir='C:\Users\gitai\Desktop\lx06-agent')
$ErrorActionPreference='Stop'
$code=Get-Content -Raw (Join-Path $ProjectDir 'tools\Invoke-LX06.ps1')
$runner=[scriptblock]::Create($code.Replace('$projectDir=Split-Path -Parent $PSScriptRoot',"`$projectDir='$ProjectDir'"))
$tag=[Guid]::NewGuid().ToString('N').Substring(0,12)
$dir="/tmp/lx06-music-test-$tag"
$script="set -eu`numask 077`nmkdir '$dir'`n"
foreach($name in @('lx06-music-armhf','lx06-music-fixture-armhf')){
 $path=Join-Path $ProjectDir "build\$name"
 $hash=(Get-FileHash -Algorithm SHA256 $path).Hash.ToLowerInvariant()
 $oct=-join([IO.File]::ReadAllBytes($path)|ForEach-Object{'\0'+[Convert]::ToString($_,8).PadLeft(3,'0')})
 $script+="printf '%b' '$oct' > '$dir/$name'`n[ `"`$(sha256sum '$dir/$name' | cut -d' ' -f1)`" = '$hash' ]`nchmod 700 '$dir/$name'`n"
}
$template=@'
D='__DIR__'
FIXTURE=''
CHANGED=0
ORIGINAL_LOOP=''
music() { printf '%s' "$1" | /lib/ld-linux-armhf.so.3 "$D/lx06-music-armhf"; }
cleanup() {
 if [ "$CHANGED" = 1 ]; then ubus -t 1 call mediaplayer player_play_operation '{"media":"common","action":"stop"}' >/dev/null 2>&1 || true; fi
 # The factory music path can leave the speaking effect active when paused.
 # Withdraw it only while our known test track still owns the player.
 current=$(ubus -t 1 call mediaplayer player_get_play_status 2>/dev/null || true)
 if printf '%s' "$current" | grep -q 'lx06-direct-stream'; then
  ubus -t 1 call led shut '{"L":3}' >/dev/null 2>&1 || true
 fi
 [ -z "$FIXTURE" ] || { kill "$FIXTURE" 2>/dev/null || true; wait "$FIXTURE" 2>/dev/null || true; }
 [ ! -f "$D/fixture.log" ] || cat "$D/fixture.log"
 rm -f "$D/lx06-music-armhf" "$D/lx06-music-fixture-armhf" "$D/fixture.log"
 rmdir "$D"
 case "$ORIGINAL_LOOP" in 1|2|3) ubus -t 1 call mediaplayer player_set_loop "{\"type\":$ORIGINAL_LOOP,\"media\":\"common\"}" >/dev/null 2>&1 || true;; esac
 echo temporary_test_removed=1
 pidof mipns-xiaomi
 pidof mico_aivs_lab
}
trap cleanup EXIT
before=$(music '{"op":"state"}')
echo "before=$before"
ORIGINAL_LOOP=$(printf '%s' "$before" | sed -n 's/.*"loop_type":\([0-9]*\).*/\1/p')
echo "$before" | grep -Eq '"status":[23]' || { echo refusing_to_interrupt_existing_playback; exit 2; }
"$D/lx06-music-fixture-armhf" > "$D/fixture.log" 2>&1 &
FIXTURE=$!
sleep 1
CHANGED=1
echo step=play
music '{"op":"play","url":"http://127.0.0.1:18087/tone.wav"}'
sleep 2
echo step=pause
music '{"op":"pause"}'
music '{"op":"state"}'
sleep 1
music '{"op":"state"}'
echo step=resume
music '{"op":"resume"}'
sleep 2
music '{"op":"state"}'
echo step=replace_track
music '{"op":"play","url":"http://127.0.0.1:18087/tone2.wav"}'
sleep 2
echo step=stop
music '{"op":"stop"}'
CHANGED=0
music '{"op":"state"}'
echo music_test=pass
'@
$script+=$template.Replace('__DIR__',$dir)
& $runner -Command $script -OutputFile (Join-Path $ProjectDir 'logs\native-music-stream-test.txt')
Get-Content (Join-Path $ProjectDir 'logs\native-music-stream-test.txt')
