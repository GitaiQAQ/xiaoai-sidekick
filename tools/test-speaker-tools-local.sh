#!/bin/sh
# All tests run on the Mac with synthetic data and fake device interfaces.
set -eu
cd "$(dirname "$0")/.."
TASK_TEST_DIR=$(mktemp -d /tmp/lx06-tools-tests.XXXXXX)
trap 'rm -rf "$TASK_TEST_DIR"' EXIT HUP INT TERM
clang -D_DEFAULT_SOURCE -std=c11 -Wall -Wextra -Werror -Wno-unused-function -framework Foundation -framework CoreFoundation tools/test-speaker-capabilities.m -o "$TASK_TEST_DIR/capabilities"
"$TASK_TEST_DIR/capabilities"
clang -D_DEFAULT_SOURCE -std=c11 -Wall -Wextra -Werror -Wno-unused-function -framework Foundation -framework CoreFoundation tools/test-tool-bridge.m -o "$TASK_TEST_DIR/bridge"
"$TASK_TEST_DIR/bridge" > "$TASK_TEST_DIR/bridge.log"
tail -n 1 "$TASK_TEST_DIR/bridge.log"
python3 tools/test-tool-endpoint.py
python3 tools/test-tools-catalog.py
python3 tools/test-lamp-feedback.py
clang -D_DEFAULT_SOURCE -std=c11 -Wall -Wextra -Werror tools/test-agent-duplex.c -o "$TASK_TEST_DIR/duplex"
"$TASK_TEST_DIR/duplex" > "$TASK_TEST_DIR/duplex.log"
tail -n 1 "$TASK_TEST_DIR/duplex.log"
clang -D_DEFAULT_SOURCE -std=c11 -Wall -Wextra -Werror tools/test-agent-timeouts.c -o "$TASK_TEST_DIR/timeouts"
"$TASK_TEST_DIR/timeouts" > "$TASK_TEST_DIR/timeouts.log"
tail -n 1 "$TASK_TEST_DIR/timeouts.log"
clang -std=c11 -Wall -Wextra -Werror src/native_session_policy_test.c -o "$TASK_TEST_DIR/policy"
"$TASK_TEST_DIR/policy"
