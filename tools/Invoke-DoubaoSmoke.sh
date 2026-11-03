#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
. "$script_dir/lx06-common.sh"
project_dir=$LX06_PROJECT_DIR credentials_file=
transport=0 realtime=0 session=0 responses=0 duplex=0 kws=0 kws_wav=0 tone=0 make_ack=0
seconds=30 kws_seconds=60
while [ "$#" -gt 0 ]; do
    case $1 in
        --project-dir) project_dir=$2; shift 2 ;;
        --credentials-file) credentials_file=$2; shift 2 ;;
        --transport-test) transport=1; shift ;;
        --realtime) realtime=1; shift ;;
        --session) session=1; shift ;;
        --responses) responses=1; shift ;;
        --duplex) duplex=1; shift ;;
        --kws) kws=1; shift ;;
        --kws-wav) kws_wav=1; shift ;;
        --tone) tone=1; shift ;;
        --make-ack) make_ack=1; shift ;;
        --seconds) seconds=$2; shift 2 ;;
        --kws-seconds) kws_seconds=$2; shift 2 ;;
        *) lx06_die "Unknown option: $1" ;;
    esac
done
for n in "$seconds" "$kws_seconds"; do case $n in ''|*[!0-9]*) lx06_die 'Durations must be integers' ;; esac; done
binary=lx06-ark-client-armhf; service=ark
if [ "$duplex" -eq 1 ]; then binary=lx06-duplex-armhf; service=realtime
elif [ "$realtime" -eq 1 ]; then binary=lx06-library-probe-armhf; service=realtime; fi

if [ "$transport" -eq 1 ]; then
    if [ "$service" = realtime ]; then payload=public-dummy-not-a-key; mode=--ws-check-key
    else payload='{"api_key":"public-dummy-not-a-key","model":"ep-test"}'; mode=--check-config; fi
else
    [ -n "$credentials_file" ] || credentials_file="$project_dir/private/credentials.json"
    [ -f "$credentials_file" ] || lx06_die 'Credential file is missing'
    [ "$(wc -c < "$credentials_file")" -le 65536 ] || lx06_die 'Credential file exceeds size limit'
    py=$(command -v python3 || command -v python || true)
    [ -n "$py" ] || lx06_die 'Python 3 is required to read credentials safely'
    if ! values=$("$py" - "$credentials_file" "$service" <<'PY'
import json, re, sys
try:
    with open(sys.argv[1], 'r', encoding='utf-8') as f: c=json.load(f)
    s=c[sys.argv[2]]
    if c.get('schema_version') != 1: raise ValueError()
    key=s.get('api_key')
    if not isinstance(key,str) or not re.fullmatch(r'[\x21-\x7e]{1,4096}',key): raise ValueError()
    model=s.get('model','')
    if sys.argv[2]=='ark' and (not isinstance(model,str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}',model)): raise ValueError()
    print(key)
    print(model)
except Exception: sys.exit(1)
PY
    ); then lx06_die 'Service key or model missing or invalid; values suppressed'; fi
    payload=${values%%$'\n'*}; model=${values#*$'\n'}
    if [ "$service" = realtime ]; then
        if [ "$tone" -eq 1 ]; then mode=--tone
        elif [ "$make_ack" -eq 1 ]; then mode='--make-ack /data/lx06-agent/wake_ack.wav'
        elif [ "$kws" -eq 1 ]; then mode="--kws $kws_seconds"
        elif [ "$kws_wav" -eq 1 ]; then mode='--kws-wav /tmp/doubao_enroll.wav'
        elif [ "$duplex" -eq 1 ]; then mode=$seconds
        elif [ "$session" -eq 1 ]; then mode=--ws-session
        else mode=--ws-stdin; fi
    else
        payload=$(printf '%s\n%s' "$payload" "$model" | "$py" -c 'import json,sys; print(json.dumps({"api_key":sys.stdin.readline().rstrip("\n"),"model":sys.stdin.readline()},separators=(",",":")))')
        if [ "$responses" -eq 1 ]; then mode=--responses; else mode=--stdin-config; fi
    fi
fi

py=$(command -v python3 || command -v python || true)
[ -n "$py" ] || lx06_die 'Python is required for ELF validation'
"$py" "$project_dir/tools/native-library-elf.py" "$binary" || lx06_die 'ELF validation failed'
local_binary="$project_dir/build/$binary"
[ -f "$local_binary" ] || lx06_die "Missing binary: $binary"
hash=$(lx06_sha256 "$local_binary")
remote="/tmp/lx06-agent-$service-$$-${RANDOM:-0}"
cleanup() {
    lx06_remote_eval "rm -f '$remote' && [ ! -e '$remote' ] && echo temporary_binary_removed=1; pidof mipns-xiaomi; pidof mico_aivs_lab; ubus call mibt_mesh mesh_status '{}'" 2>/dev/null || true
}
trap cleanup EXIT
lx06_ssh "umask 077; cat > '$remote'" < "$local_binary" || lx06_die 'Binary upload failed'
remote_hash=$(lx06_remote_eval "sha256sum '$remote' | cut -d' ' -f1")
[ "$remote_hash" = "$hash" ] || lx06_die 'Remote binary SHA-256 mismatch'
lx06_remote_eval "chmod 700 '$remote'"

limit=35000
if [ "$duplex" -eq 1 ]; then max=$seconds; [ "$kws_seconds" -gt "$max" ] && max=$kws_seconds; limit=$(((max+60)*1000)); fi
timeout_bin=$(command -v timeout || command -v gtimeout || true)
[ -n "$timeout_bin" ] || lx06_die 'GNU timeout (timeout or gtimeout) is required for bounded smoke operations'
set +e
output=$(printf '%s' "$payload" | "$timeout_bin" --signal=KILL "$((limit/1000))" ssh -T -o HostKeyAlgorithms=+ssh-rsa -o "UserKnownHostsFile=$LX06_KNOWN_HOSTS" -o ConnectTimeout=8 -o NumberOfPasswordPrompts=1 "$LX06_HOST" "exec /lib/ld-linux-armhf.so.3 $remote $mode" 2>/dev/null)
rc=$?
set -e
log="$project_dir/logs"
mkdir -p -- "$log"
if [ "$transport" -eq 1 ]; then log_name="$service-stdin-transport-test.txt"
elif [ "$duplex" -eq 1 ] && [ "$kws_wav" -eq 1 ]; then log_name=kws-session-runtime.txt
elif [ "$duplex" -eq 1 ]; then log_name=duplex-runtime.txt
else log_name="$service-auth-smoke.txt"; fi
printf '%s' "$output" > "$log/$log_name"
printf '%s' "$output"
unset payload values model
[ "$rc" -eq 0 ] || lx06_die 'Operation failed or timed out; inspect metadata log (secret output suppressed)'
