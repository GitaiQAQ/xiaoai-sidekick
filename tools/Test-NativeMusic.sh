#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd -- "$script_dir/.." && pwd)
. "$script_dir/lx06-common.sh"
if (($#)); then
  if (($# == 2)) && [[ $1 == --project-dir || $1 == -ProjectDir ]]; then project_dir=$2
  else echo 'Usage: Test-NativeMusic.sh [--project-dir DIR]' >&2; exit 2; fi
fi
project_dir=$(cd -- "$project_dir" && pwd)
python=${LX06_PYTHON:-}
if [[ -z $python ]]; then for c in python3 python; do if command -v "$c" >/dev/null 2>&1; then python=$c; break; fi; done; fi
[[ -n $python ]] || { echo 'python3 or python is required' >&2; exit 1; }
if command -v cygpath >/dev/null 2>&1; then
  music_file=$(cygpath -w "$project_dir/build/lx06-music-armhf")
  fixture_file=$(cygpath -w "$project_dir/build/lx06-music-fixture-armhf")
else
  music_file="$project_dir/build/lx06-music-armhf"
  fixture_file="$project_dir/build/lx06-music-fixture-armhf"
fi
for f in "$music_file" "$fixture_file"; do [[ -f $f ]] || { echo "missing binary: $f" >&2; exit 1; }; done
if command -v sha256sum >/dev/null 2>&1; then
  music_hash=$(sha256sum "$project_dir/build/lx06-music-armhf" | awk '{print tolower($1)}')
  fixture_hash=$(sha256sum "$project_dir/build/lx06-music-fixture-armhf" | awk '{print tolower($1)}')
elif command -v shasum >/dev/null 2>&1; then
  music_hash=$(shasum -a 256 "$project_dir/build/lx06-music-armhf" | awk '{print tolower($1)}')
  fixture_hash=$(shasum -a 256 "$project_dir/build/lx06-music-fixture-armhf" | awk '{print tolower($1)}')
else
  echo 'sha256sum or shasum is required' >&2
  exit 1
fi
music_oct=$($python -c 'import sys; print("".join("\\0%03o" % b for b in open(sys.argv[1], "rb").read()), end="")' "$music_file")
fixture_oct=$($python -c 'import sys; print("".join("\\0%03o" % b for b in open(sys.argv[1], "rb").read()), end="")' "$fixture_file")
tag=$(od -An -N6 -tx1 /dev/urandom | tr -d ' \n')
remote_dir="/tmp/lx06-music-test-$tag"
output="$project_dir/logs/native-music-stream-test.txt"
mkdir -p "$(dirname -- "$output")"
ssh_bin=${LX06_SSH:-ssh}
ssh_opts=(-T -o HostKeyAlgorithms=+ssh-rsa -o "UserKnownHostsFile=$project_dir/tools/local/known_hosts" -o ConnectTimeout=8 -o NumberOfPasswordPrompts=1)
remote_script=$(cat <<EOF
set -eu
umask 077
D='$remote_dir'
if [ -e "\$D" ]; then echo 'refusing to reuse existing remote test directory' >&2; exit 1; fi
mkdir "\$D"
trap 'rm -f "\$D/lx06-music-armhf" "\$D/lx06-music-fixture-armhf" "\$D/fixture.log"; rmdir "\$D" 2>/dev/null || true' 0
printf '%b' '$music_oct' > "\$D/lx06-music-armhf"
actual=\$(sha256sum "\$D/lx06-music-armhf" | cut -d' ' -f1)
[ "\$actual" = '$music_hash' ] || { echo 'music binary checksum mismatch' >&2; exit 1; }
chmod 700 "\$D/lx06-music-armhf"
printf '%b' '$fixture_oct' > "\$D/lx06-music-fixture-armhf"
actual=\$(sha256sum "\$D/lx06-music-fixture-armhf" | cut -d' ' -f1)
[ "\$actual" = '$fixture_hash' ] || { echo 'fixture binary checksum mismatch' >&2; exit 1; }
chmod 700 "\$D/lx06-music-fixture-armhf"
D='$remote_dir'
FIXTURE=''
CHANGED=0
ORIGINAL_LOOP=''
music() { printf '%s' "\$1" | /lib/ld-linux-armhf.so.3 "\$D/lx06-music-armhf"; }
cleanup() {
 if [ "\$CHANGED" = 1 ]; then ubus -t 1 call mediaplayer player_play_operation '{"media":"common","action":"stop"}' >/dev/null 2>&1 || true; fi
 current=\$(ubus -t 1 call mediaplayer player_get_play_status 2>/dev/null || true)
 if printf '%s' "\$current" | grep -q 'lx06-direct-stream'; then ubus -t 1 call led shut '{"L":3}' >/dev/null 2>&1 || true; fi
 [ -z "\$FIXTURE" ] || { kill "\$FIXTURE" 2>/dev/null || true; wait "\$FIXTURE" 2>/dev/null || true; }
 [ ! -f "\$D/fixture.log" ] || cat "\$D/fixture.log"
 rm -f "\$D/lx06-music-armhf" "\$D/lx06-music-fixture-armhf" "\$D/fixture.log"
 rmdir "\$D"
 case "\$ORIGINAL_LOOP" in 1|2|3) ubus -t 1 call mediaplayer player_set_loop "{\"type\":\$ORIGINAL_LOOP,\"media\":\"common\"}" >/dev/null 2>&1 || true;; esac
 echo temporary_test_removed=1
 pidof mipns-xiaomi
 pidof mico_aivs_lab
}
trap cleanup 0
before=\$(music '{"op":"state"}')
echo "before=\$before"
ORIGINAL_LOOP=\$(printf '%s' "\$before" | sed -n 's/.*"loop_type":\([0-9]*\).*/\1/p')
echo "\$before" | grep -Eq '"status":[23]' || { echo refusing_to_interrupt_existing_playback; exit 2; }
"\$D/lx06-music-fixture-armhf" > "\$D/fixture.log" 2>&1 &
FIXTURE=\$!
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
EOF
)
set +e
printf '%s\n' "$remote_script" | "$ssh_bin" "${ssh_opts[@]}" root@192.168.100.210 sh 2>&1 | tee "$output"
rc=${PIPESTATUS[1]}
set -e
exit "$rc"
