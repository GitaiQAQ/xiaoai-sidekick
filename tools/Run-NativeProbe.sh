#!/usr/bin/env bash
set -euo pipefail
usage() { echo 'Usage: Run-NativeProbe.sh [--project-dir DIR] [--kind KIND] [--thread-id N] [--socket-fd N] [--seconds N]'; }
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd -- "$script_dir/.." && pwd)
. "$script_dir/lx06-common.sh"
kind=probe thread_id=2601 socket_fd=8 seconds=5
while (($#)); do
  case "$1" in
    --project-dir|-ProjectDir) project_dir=$2; shift 2 ;;
    --kind|-Kind) kind=$2; shift 2 ;;
    --thread-id|-ThreadId) thread_id=$2; shift 2 ;;
    --socket-fd|-SocketFd) socket_fd=$2; shift 2 ;;
    --seconds|-Seconds) seconds=$2; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done
project_dir=$(cd -- "$project_dir" && pwd)
case "$kind" in probe|socket-test|observer-test|observer|libraries|tls|ark-test|ws-transport|ws-test|ws-loopback|afe-frame|kws-enroll|feedback-test|kws-score|kws-live|kws-capture|kws-replay|kws-standby|kws-signal) ;; *) echo "invalid kind: $kind" >&2; exit 2;; esac
[[ $thread_id =~ ^[0-9]+$ ]] || { echo 'ThreadId must be 2..4194304' >&2; exit 2; }
[[ $socket_fd =~ ^[0-9]+$ ]] || { echo 'SocketFd must be 0..1024' >&2; exit 2; }
[[ $seconds =~ ^[0-9]+$ ]] || { echo 'Seconds must be 1..300' >&2; exit 2; }
thread_id=$((10#$thread_id)); socket_fd=$((10#$socket_fd)); seconds=$((10#$seconds))
((thread_id >= 2 && thread_id <= 4194304)) || { echo 'ThreadId must be 2..4194304' >&2; exit 2; }
((socket_fd <= 1024)) || { echo 'SocketFd must be 0..1024' >&2; exit 2; }
((seconds >= 1 && seconds <= 300)) || { echo 'Seconds must be 1..300' >&2; exit 2; }
case "$kind" in
  probe) name=lx06-probe-armhf ;;
  socket-test) name=lx06-socket-test-armhf ;;
  libraries|tls|ws-transport|ws-test|ws-loopback) name=lx06-library-probe-armhf ;;
  ark-test) name=lx06-ark-client-armhf ;;
  afe-frame) name=lx06-afe-frame-armhf ;;
  kws-enroll) name=lx06-kws-enroll-armhf ;;
  feedback-test|kws-score|kws-live|kws-capture|kws-replay|kws-standby|kws-signal) name=lx06-duplex-armhf ;;
  *) name=lx06-speech-observer-armhf ;;
esac
file="$project_dir/build/$name"
[[ -f $file ]] || { echo "missing binary: $file" >&2; exit 1; }
python=${LX06_PYTHON:-}
if [[ -z $python ]]; then for c in python3 python; do if command -v "$c" >/dev/null 2>&1; then python=$c; break; fi; done; fi
[[ -n $python ]] || { echo 'python3 or python is required' >&2; exit 1; }
case "$kind" in libraries|tls|ark-test|ws-transport|ws-test|ws-loopback|afe-frame|kws-enroll|feedback-test|kws-score|kws-live|kws-capture|kws-replay|kws-standby|kws-signal) "$python" "$project_dir/tools/native-library-elf.py" "$name";; esac
if command -v sha256sum >/dev/null 2>&1; then hash=$(sha256sum "$file" | awk '{print tolower($1)}')
elif command -v shasum >/dev/null 2>&1; then hash=$(shasum -a 256 "$file" | awk '{print tolower($1)}')
else echo 'sha256sum or shasum is required' >&2; exit 1; fi
remote="/tmp/lx06-agent-$kind-${hash:0:12}"
if command -v cygpath >/dev/null 2>&1; then pyfile=$(cygpath -w "$file"); else pyfile=$file; fi
octal=$($python -c 'import sys; print("".join("\\0%03o" % b for b in open(sys.argv[1], "rb").read()), end="")' "$pyfile")

case "$kind" in
  probe) run='"$dst" --self-test
"$dst" --status' ;;
  socket-test) run='"$dst"' ;;
  libraries) run='strace -f -s 0 -e trace=network -o "$dst.trace" /lib/ld-linux-armhf.so.3 "$dst"
cat "$dst.trace"
rm -f "$dst.trace"' ;;
  ark-test) run='/lib/ld-linux-armhf.so.3 "$dst" --self-test' ;;
  ws-test) run='/lib/ld-linux-armhf.so.3 "$dst" --ws-self-test' ;;
  ws-transport) run='/lib/ld-linux-armhf.so.3 "$dst" --ws-probe' ;;
  afe-frame) run='/lib/ld-linux-armhf.so.3 "$dst"' ;;
  kws-enroll) run='/lib/ld-linux-armhf.so.3 "$dst" /tmp/doubao_enroll.wav' ;;
  feedback-test) run='timeout -t 12 /lib/ld-linux-armhf.so.3 "$dst" --feedback-test' ;;
  kws-score) run='/lib/ld-linux-armhf.so.3 "$dst" --kws-score /tmp/doubao_enroll.wav' ;;
  kws-live) run="timeout -t $((seconds+8)) /lib/ld-linux-armhf.so.3 \"\$dst\" --kws-live $seconds" ;;
  kws-capture) run="timeout -t $((seconds+8)) /lib/ld-linux-armhf.so.3 \"\$dst\" --kws-capture $seconds" ;;
  kws-replay) run='/lib/ld-linux-armhf.so.3 "$dst" --kws-wav /tmp/doubao_enroll.wav < /dev/null' ;;
  kws-signal) run="if timeout -t $seconds /lib/ld-linux-armhf.so.3 \"\$dst\" --kws 300 < /dev/null; then echo signal_exit=0; else echo signal_exit=\$?; fi" ;;
  kws-standby) run="if timeout -t $((seconds+10)) /lib/ld-linux-armhf.so.3 \"\$dst\" --kws $seconds < /dev/null; then echo standby_exit=0; else echo standby_exit=\$?; fi" ;;
  ws-loopback) run='LD=/lib/ld-linux-armhf.so.3
run_case() {
 scenario=$1
 "$LD" "$dst" --ws-test-server 18099 "$scenario" > "/tmp/lx06-ws-srv-$scenario.log" 2>&1 & srv=$!
 sleep 1
 set +e; "$LD" "$dst" --ws-local 18099; rc=$?; wait "$srv"; src=$?; set -e
 echo "CASE $scenario client_rc=$rc server_rc=$src"
 cat "/tmp/lx06-ws-srv-$scenario.log"; rm -f "/tmp/lx06-ws-srv-$scenario.log"
}
run_case good
run_case badaccept
run_case status400
run_case silent' ;;
  tls) run='/lib/ld-linux-armhf.so.3 "$dst" --tls' ;;
  observer-test) run='"$dst" --self-test' ;;
  observer) run="\"\$dst\" --observe $thread_id $socket_fd $seconds" ;;
esac

output="$project_dir/logs/native-$kind-runtime.txt"
mkdir -p "$(dirname -- "$output")"
ssh_bin=${LX06_SSH:-ssh}
ssh_opts=(-T -o HostKeyAlgorithms=+ssh-rsa -o "UserKnownHostsFile=$project_dir/tools/local/known_hosts" -o ConnectTimeout=8 -o NumberOfPasswordPrompts=1)
remote_script=$(cat <<EOF
set -eu
dst='$remote'
if [ -e "\$dst" ] || [ -e "\$dst.trace" ]; then echo 'refusing to overwrite existing probe path' >&2; exit 1; fi
umask 077
trap 'rm -f "\$dst" "\$dst.trace"' 0
echo native_processes_before
pidof mipns-xiaomi
pidof mico_aivs_lab
printf '%b' '$octal' > "\$dst"
actual=\$(sha256sum "\$dst"); actual=\${actual%% *}
if [ "\$actual" != '$hash' ]; then echo 'remote checksum mismatch' >&2; exit 1; fi
chmod 700 "\$dst"
$run
rm -f "\$dst"
[ ! -e "\$dst" ]
echo temporary_binary_removed=1
echo native_processes_after
pidof mipns-xiaomi
pidof mico_aivs_lab
EOF
)
set +e
printf '%s\n' "$remote_script" | "$ssh_bin" "${ssh_opts[@]}" root@192.168.100.210 sh 2>&1 | tee "$output"
rc=${PIPESTATUS[1]}
set -e
exit "$rc"
