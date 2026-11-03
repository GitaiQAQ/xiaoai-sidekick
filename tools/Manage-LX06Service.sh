#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
. "$script_dir/lx06-common.sh"
project_dir=$LX06_PROJECT_DIR
start=0 stop=0 status=0 force_upload=0 fake_key=0 standby=60
while [ "$#" -gt 0 ]; do
    case $1 in
        --project-dir) project_dir=$2; shift 2 ;;
        --start) start=1; shift ;;
        --stop) stop=1; shift ;;
        --status) status=1; shift ;;
        --force-upload) force_upload=1; shift ;;
        --fake-key) fake_key=1; shift ;;
        --standby) standby=$2; shift 2 ;;
        *) lx06_die "Unknown option: $1" ;;
    esac
done
case $standby in ''|*[!0-9]*) lx06_die 'Standby must be an integer from 15 to 600' ;; esac
[ "$standby" -ge 15 ] && [ "$standby" -le 600 ] || lx06_die 'Standby must be an integer from 15 to 600'
dir=/data/lx06-agent

remote_command() {
    local command=$1 input=${2-}
    if [ -n "$input" ]; then printf '%s\n' "$input" | lx06_ssh "$command"; else lx06_ssh "$command" </dev/null; fi
}
remote_script() { printf '%s\n' "$1" | lx06_remote_sh; }

if [ "$status" -eq 1 ] || [ "$stop" -eq 1 ]; then
    if [ "$stop" -eq 1 ]; then remote_command "exec $dir/stop.sh"; fi
    if [ "$status" -eq 1 ]; then remote_command "exec $dir/status.sh"; fi
    exit 0
fi

remote_script "mkdir -p '$dir' && chmod 700 '$dir' && [ -d '$dir' ] && echo dir_ready"
files=(lx06-duplex-armhf deploy/start.sh deploy/stop.sh deploy/status.sh)
for rel in "${files[@]}"; do
    local_file="$project_dir/$rel"
    case $rel in deploy/*) name=${rel##*/} ;; *) name=$rel ;; esac
    [ -f "$local_file" ] || lx06_die "Missing deployment file: $rel"
    hash=$(lx06_sha256 "$local_file")
    remote_hash=$(lx06_remote_eval "sha256sum '$dir/$name' 2>/dev/null | cut -d' ' -f1" || true)
    if [ "$remote_hash" = "$hash" ] && [ "$force_upload" -eq 0 ]; then
        printf 'unchanged %s\n' "$name"; continue
    fi
    # Stream the file as raw bytes; no command-line expansion or text conversion.
    if ! lx06_ssh "umask 077; cat > '$dir/$name.tmp'" < "$local_file"; then lx06_die "Upload failed: $name"; fi
    verify=$(lx06_remote_eval "sha256sum '$dir/$name.tmp' | cut -d' ' -f1")
    if [ "$verify" != "$hash" ]; then
        lx06_remote_eval "rm -f '$dir/$name.tmp'" >/dev/null 2>&1 || true
        lx06_die "Remote SHA-256 mismatch: $name"
    fi
    lx06_remote_eval "chmod 700 '$dir/$name.tmp' && mv '$dir/$name.tmp' '$dir/$name' && echo uploaded=$name" >/dev/null
    printf 'uploaded %s\n' "$name"
done

if [ "$start" -eq 1 ]; then
    if [ "$fake_key" -eq 1 ]; then key=public-dummy-not-a-key
    else
        cred="$project_dir/private/credentials.json"
        [ -f "$cred" ] || lx06_die 'realtime key missing'
        key=$(lx06_json_value "$cred" realtime.api_key) || lx06_die 'realtime key missing'
    fi
    remote_command "exec $dir/start.sh $standby" "$key"
    unset key
    sleep 2
    remote_command "exec $dir/status.sh"
fi
