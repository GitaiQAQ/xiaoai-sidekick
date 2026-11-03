#!/usr/bin/env bash
# Shared Git Bash/Linux helpers for LX06 operations.

LX06_TOOLS_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
LX06_PROJECT_DIR=$(cd -- "$LX06_TOOLS_DIR/.." && pwd)
LX06_HOST=${LX06_HOST:-root@192.168.100.210}
LX06_KNOWN_HOSTS=${LX06_KNOWN_HOSTS:-"$LX06_TOOLS_DIR/local/known_hosts"}
if [ -n "${LX06_ASKPASS:-}" ]; then :
elif [ "${OS:-}" = Windows_NT ]; then LX06_ASKPASS="$LX06_TOOLS_DIR/local/ssh-askpass.cmd"
else LX06_ASKPASS="$LX06_TOOLS_DIR/ssh-askpass.sh"; fi
if [ "${OS:-}" = Windows_NT ] && command -v cygpath >/dev/null 2>&1; then
    LX06_ASKPASS=$(cygpath -w "$LX06_ASKPASS")
fi
export SSH_ASKPASS="$LX06_ASKPASS" SSH_ASKPASS_REQUIRE=force DISPLAY=${DISPLAY:-codex}

lx06_die() { printf '%s\n' "$*" >&2; exit 1; }
lx06_need() { command -v "$1" >/dev/null 2>&1 || lx06_die "Required command not found: $1"; }

lx06_ssh() {
    ssh -T \
        -o HostKeyAlgorithms=+ssh-rsa \
        -o "UserKnownHostsFile=$LX06_KNOWN_HOSTS" \
        -o ConnectTimeout=8 \
        -o NumberOfPasswordPrompts=1 \
        "$LX06_HOST" "$@"
}

# Send a shell program to the remote Bourne shell without placing its contents
# in SSH's remote-command argument.
lx06_remote_sh() { lx06_ssh sh; }
lx06_remote_eval() { printf '%s\n' "$1" | lx06_ssh sh; }

lx06_sha256() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print tolower($1)}'
    elif command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | awk '{print tolower($1)}'
    else lx06_die 'sha256sum or shasum is required'; fi
}

lx06_json_value() {
    # Usage: lx06_json_value FILE dotted.path; output is one validated value.
    local json_file=$1 dotted_key=$2 py
    py=$(command -v python3 || command -v python || true)
    [ -n "$py" ] || lx06_die 'Python 3 is required to read credentials safely'
    "$py" - "$json_file" "$dotted_key" <<'PY'
import json, re, sys
try:
    with open(sys.argv[1], 'r', encoding='utf-8') as f:
        data = json.load(f)
    for part in sys.argv[2].split('.'):
        data = data[part]
    if not isinstance(data, str) or not re.fullmatch(r'[\x21-\x7e]{1,4096}', data):
        raise ValueError()
    sys.stdout.write(data)
except Exception:
    sys.exit(1)
PY
}

