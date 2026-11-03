#!/bin/sh
# Stop only the verified client and tagged key writer, never a parent shell.
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
BIN="$DIR/lx06-duplex-armhf"
is_client() {
 case "${1:-}" in ''|*[!0-9]*) return 1;; esac
 [ -r "/proc/$1/cmdline" ] || return 1
 tr '\000' '\n' 2>/dev/null < "/proc/$1/cmdline" | grep -Fx "$BIN" >/dev/null
}
if [ -f "$DIR/lx06.pid" ]; then
 pid=$(cat "$DIR/lx06.pid" 2>/dev/null || true)
 if is_client "$pid"; then kill "$pid" 2>/dev/null || true; fi
 rm -f "$DIR/lx06.pid"
fi
if [ -f "$DIR/writer.pid" ]; then
 pid=$(cat "$DIR/writer.pid" 2>/dev/null || true)
 case "$pid" in ''|*[!0-9]*) ;; *)
  if [ "$(cat "/proc/$pid/comm" 2>/dev/null || true)" = lx06-keywriter ]; then kill "$pid" 2>/dev/null || true; fi;;
 esac
 rm -f "$DIR/writer.pid"
fi
for pid in $(pidof ld-linux-armhf.so.3 2>/dev/null || true); do
 if is_client "$pid"; then kill "$pid" 2>/dev/null || true; fi
done
rm -f "$DIR/key.fifo"
sleep 1
for pid in $(pidof ld-linux-armhf.so.3 2>/dev/null || true); do
 if is_client "$pid"; then echo still_running >&2; exit 1; fi
done
echo stopped
