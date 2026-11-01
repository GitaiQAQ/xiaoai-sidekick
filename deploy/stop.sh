#!/bin/sh
# Stop the standby client and its key writer. Safe to run when nothing is up.
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
for f in lx06.pid writer.pid; do
    if [ -f "$DIR/$f" ]; then
        pid=$(cat "$DIR/$f" 2>/dev/null || true)
        if [ -n "${pid:-}" ]; then
            kill "$pid" 2>/dev/null || true
        fi
        rm -f "$DIR/$f"
    fi
done
# Fallback for a process that outlived its pid file.
ps w 2>/dev/null | grep -F "$DIR/lx06-duplex-armhf" | grep -v grep |
while read -r pid rest; do
    kill "$pid" 2>/dev/null || true
done
rm -f "$DIR/key.fifo"
sleep 1
if ps w 2>/dev/null | grep -F "$DIR/lx06-duplex-armhf" | grep -v grep >/dev/null; then
    echo "still_running" >&2
    exit 1
fi
echo "stopped"
