#!/bin/sh
# Show whether the standby client is running and whether it holds a cloud link.
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
if [ -f "$DIR/lx06.pid" ]; then
    pid=$(cat "$DIR/lx06.pid" 2>/dev/null || true)
    echo "pidfile=$pid"
fi
ps w 2>/dev/null | grep -F "$DIR/lx06-duplex-armhf" | grep -v grep || true
netstat -tn 2>/dev/null | grep -E "157\.255\.|openspeech" || true
[ -f "$DIR/client.log" ] && tail -n 5 "$DIR/client.log"
exit 0
