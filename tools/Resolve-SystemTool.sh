#!/usr/bin/env bash
# Source this file, then call resolve_system_tool NAME.

resolve_system_tool() {
    if [[ $# -ne 1 || -z $1 ]]; then
        printf 'usage: resolve_system_tool NAME\n' >&2
        return 2
    fi

    local name=$1 found candidate entry
    case "$name" in
        zig.exe)
            if [[ -n ${LX06_ZIG:-} ]]; then
                name=$LX06_ZIG
            else
                name=zig
            fi
            ;;
        python.exe)
            if [[ -n ${LX06_PYTHON:-} ]]; then
                name=$LX06_PYTHON
            else
                name=python
            fi
            ;;
    esac

    # Prefer an installed executable over the WindowsApps Python stub, which
    # can appear in command -v without providing an interpreter.
    local candidates=("$name")
    case "$name" in
        zig) candidates+=(zig.exe) ;;
        python) candidates+=(python3 python.exe python3.exe) ;;
        python3) candidates+=(python3.exe python python.exe) ;;
        python.exe|python3.exe) candidates+=(python3 python python3.exe python.exe) ;;
    esac
    local IFS=:
    for entry in $PATH; do
        case "${entry//\\//}" in
            */Microsoft/WindowsApps) continue ;;
        esac
        for candidate in "${candidates[@]}"; do
            if [[ -x $entry/$candidate ]]; then
                printf '%s\n' "$entry/$candidate"
                return 0
            fi
        done
    done

    for candidate in "${candidates[@]}"; do
        found=$(command -v "$candidate" 2>/dev/null) || found=
        if [[ -n $found ]]; then
            printf '%s\n' "$found"
            return 0
        fi
    done
    printf "Required system tool '%s' was not found on PATH.\n" "$name" >&2
    return 1
}
