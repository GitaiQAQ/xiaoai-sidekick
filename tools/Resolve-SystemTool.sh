#!/usr/bin/env bash
# Source this file, then call resolve_system_tool NAME.

resolve_system_tool() {
    if [[ $# -ne 1 || -z $1 ]]; then
        printf 'usage: resolve_system_tool NAME\n' >&2
        return 2
    fi

    local name=$1 found
    case "$name" in
        zig) name=${LX06_ZIG:-zig} ;;
        python3) name=${LX06_PYTHON:-python3} ;;
    esac
    found=$(command -v "$name" 2>/dev/null) || found=
    if [[ -n $found ]]; then
        printf '%s\n' "$found"
        return 0
    fi
    printf "Required system tool '%s' was not found on PATH.\n" "$name" >&2
    return 1
}
