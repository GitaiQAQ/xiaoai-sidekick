#!/usr/bin/env bash
# Backwards-compatible text-only entry point.
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec "$script_dir/Invoke-DoubaoSmoke.sh" "$@"
