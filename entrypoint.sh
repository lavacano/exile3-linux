#!/bin/bash
set -e

# If user wants a shell, pass through directly
if [ "$1" = "bash" ] || [ "$1" = "sh" ]; then
    exec "$@"
fi

TARGET="${1:-exile3}"

if [ "$TARGET" = "editor" ] || [ "$TARGET" = "exile3ed" ]; then
    BIN="/game/exile3ed-binary"
    TITLE="Exile III Character Editor"
else
    BIN="/game/exile3-binary"
    TITLE="Exile III: Ruined World"
fi

# Find an available nested X display number
NESTED_DISP=10
while [ -e "/tmp/.X11-unix/X$NESTED_DISP" ]; do
    NESTED_DISP=$((NESTED_DISP + 1))
done

DEPTH=24
if [ "$EXILE_8BIT" = "1" ]; then
    DEPTH=8
fi

echo "Starting Xephyr ${DEPTH}-bit TrueColor server on :$NESTED_DISP..."
Xephyr ":$NESTED_DISP" -screen "640x480x$DEPTH" -title "$TITLE" -ac -fp /game/fonts &
XEP_PID=$!

cleanup() {
    echo "Shutting down..."
    kill -9 $XEP_PID 2>/dev/null || true
    rm -f "/tmp/.X11-unix/X$NESTED_DISP" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# Wait for Xephyr to be ready
for i in $(seq 1 50); do
    if xdpyinfo -display ":$NESTED_DISP" >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

export DISPLAY=":$NESTED_DISP"
export EXILE_PATH="/game"
export LD_LIBRARY_PATH="/game"
export LD_PRELOAD="/game/libexile3audio.so"

echo "Launching $TITLE..."
padsp "$BIN"
cleanup
