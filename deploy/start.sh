#!/bin/sh
# Standby service launcher. Reads the API key from stdin and keeps it in a
# FIFO for the daemon to consume at startup; the key never appears in argv
# or in a regular file. Local wake detection does not open a cloud session.
set -eu
DIR=$(cd "$(dirname "$0")" && pwd)
BIN="$DIR/lx06-duplex-armhf"
FIFO="$DIR/key.fifo"
LOG="$DIR/client.log"
SECONDS_ARG="${1:-60}"
[ -x "$BIN" ] || { echo "missing $BIN" >&2; exit 2; }
"$DIR/stop.sh" >/dev/null 2>&1 || true
rm -f "$FIFO"
mkfifo -m 600 "$FIFO"
# POSIX sh gives background jobs /dev/null as stdin, so read the key in the
# foreground and hand it to the FIFO from a writer that needs no stdin.
KEY=$(cat)
if [ -z "${KEY:-}" ]; then echo "empty key on stdin" >&2; rm -f "$FIFO"; exit 2; fi
printf '%s\n' "$KEY" > "$FIFO" &
WRITER=$!
unset KEY
echo "$WRITER" > "$DIR/writer.pid"
export LX_SESSION_MAX="${LX_SESSION_MAX:-60}"
export LX_FOLLOWUP="${LX_FOLLOWUP:-8}"
export LX_NO_SPEECH="${LX_NO_SPEECH:-6}"
/lib/ld-linux-armhf.so.3 "$BIN" --kws "$SECONDS_ARG" --daemon "$DIR/lx06.pid" < "$FIFO" > "$LOG" 2>&1
sleep 1
PID=$(cat "$DIR/lx06.pid" 2>/dev/null || true)
echo "started pid=${PID:-none} writer=$WRITER standby=$SECONDS_ARG session_max=$LX_SESSION_MAX no_speech=$LX_NO_SPEECH"
