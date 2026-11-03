#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
. "$script_dir/lx06-common.sh"
project_dir=$LX06_PROJECT_DIR install=0 start=0 stop=0 status=0 no_voice_key=0
while [ "$#" -gt 0 ]; do
    case $1 in
        --project-dir) project_dir=$2; shift 2 ;;
        --install) install=1; shift ;;
        --start) start=1; shift ;;
        --stop) stop=1; shift ;;
        --status) status=1; shift ;;
        --no-voice-key) no_voice_key=1; shift ;;
        *) lx06_die "Unknown option: $1" ;;
    esac
done
dir=/data/lx06-agent
remote_script() { printf '%s\n' "$1" | lx06_remote_sh; }

if [ "$stop" -eq 1 ] || [ "$install" -eq 1 ]; then
    stop_code='set -eu; f=/data/lx06-agent/web.pid; if [ -f "$f" ]; then p=$(cat "$f"); case "$p" in ""|*[!0-9]*) echo invalid_pid >&2; exit 1;; esac; if [ -r "/proc/$p/cmdline" ] && tr "\\000" "\\n" < "/proc/$p/cmdline" | grep -Fxq "/data/lx06-agent/lx06-web-armhf"; then kill "$p"; n=0; while kill -0 "$p" 2>/dev/null && [ "$n" -lt 12 ]; do sleep 1; n=$((n+1)); done; if kill -0 "$p" 2>/dev/null; then echo web_stop_pending; exit 1; fi; fi; rm -f "$f"; fi; echo web_stopped'
    remote_script "$stop_code"
fi

if [ "$install" -eq 1 ]; then
    py=$(command -v python3 || command -v python || true)
    [ -n "$py" ] || lx06_die 'Python is required for ELF validation'
    "$py" "$project_dir/tools/native-library-elf.py" lx06-web-armhf || lx06_die 'ELF validation failed'
    remote_script "set -eu; mkdir -p '$dir/web'; chmod 700 '$dir' '$dir/web'"
    files=(lx06-web-armhf lx06-duplex-armhf web/index.html web/app.css web/app.js web/qrcode.js web/QRCODE-LICENSE.txt)
    for rel in "${files[@]}"; do
        local_file="$project_dir/$rel"
        [ -f "$local_file" ] || lx06_die "Missing deployment file: $rel"
        hash=$(lx06_sha256 "$local_file")
        remote_path="$dir/$rel"
        existing=$(lx06_remote_eval "sha256sum '$remote_path' 2>/dev/null | cut -d' ' -f1" || true)
        if [ "$existing" = "$hash" ]; then printf 'unchanged %s\n' "$rel"; continue; fi
        if [ "$rel" = lx06-duplex-armhf ]; then
            voice=$(lx06_remote_eval 'for f in /proc/[0-9]*/cmdline; do tr "\000" "\n" < "$f" 2>/dev/null | grep -Fxq "/data/lx06-agent/lx06-duplex-armhf" && echo voice_running && break; done')
            [ -z "$voice" ] || lx06_die 'Stop voice assistant before updating its binary'
        fi
        lx06_ssh "umask 077; mkdir -p '$dir/$(dirname -- "$rel")'; cat > '$remote_path.new'" < "$local_file" || lx06_die "Upload failed: $rel"
        verify=$(lx06_remote_eval "sha256sum '$remote_path.new' | cut -d' ' -f1")
        if [ "$verify" != "$hash" ]; then
            lx06_remote_eval "rm -f '$remote_path.new'" >/dev/null 2>&1 || true
            lx06_die "Remote SHA-256 mismatch: $rel"
        fi
        mode=600; [[ "$rel" == *armhf ]] && mode=700
        lx06_remote_eval "chmod $mode '$remote_path.new' && mv '$remote_path.new' '$remote_path' && echo installed=$rel" >/dev/null
        printf 'installed %s\n' "$rel"
    done
fi

if [ "$start" -eq 1 ]; then
    key=
    if [ "$no_voice_key" -eq 0 ] && [ -f "$project_dir/private/credentials.json" ]; then
        key=$(lx06_json_value "$project_dir/private/credentials.json" realtime.api_key 2>/dev/null || true)
    fi
    remote_command='exec /lib/ld-linux-armhf.so.3 /data/lx06-agent/lx06-web-armhf --daemon > /data/lx06-agent/web.log 2>&1'
    printf '%s\n' "$key" | lx06_ssh "$remote_command" || lx06_die 'Web service failed to start; inspect metadata log'
    unset key
    printf '%s\n' web_start_requested
fi

if [ "$status" -eq 1 ] || [ "$start" -eq 1 ]; then
    remote_script 'cat /data/lx06-agent/web.pid 2>/dev/null || true; tail -n 2 /data/lx06-agent/web.log 2>/dev/null || true'
fi
