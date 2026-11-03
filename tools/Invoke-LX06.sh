#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
. "$script_dir/lx06-common.sh"

command_text= output_file= binary=0
while [ "$#" -gt 0 ]; do
    case $1 in
        --command) [ "$#" -ge 2 ] || lx06_die '--command requires a value'; command_text=$2; shift 2 ;;
        --output-file) [ "$#" -ge 2 ] || lx06_die '--output-file requires a path'; output_file=$2; shift 2 ;;
        --binary) binary=1; shift ;;
        --project-dir) [ "$#" -ge 2 ] || lx06_die '--project-dir requires a path'; LX06_PROJECT_DIR=$2; shift 2 ;;
        *) lx06_die "Unknown option: $1" ;;
    esac
done
[ -n "$command_text" ] || lx06_die '--command is required'
if [ "$binary" -eq 1 ] && [ -z "$output_file" ]; then lx06_die '--binary requires --output-file'; fi

if [ -n "$output_file" ]; then
    mkdir -p -- "$(dirname -- "$output_file")"
    if [ "$binary" -eq 1 ]; then
        printf '%s\n' "${command_text//$'\r'/}" | lx06_ssh sh > "$output_file"
    else
        printf '%s\n' "${command_text//$'\r'/}" | lx06_ssh sh > "$output_file"
    fi
else
    printf '%s\n' "${command_text//$'\r'/}" | lx06_ssh sh
fi
