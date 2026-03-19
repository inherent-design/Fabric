#!/bin/sh
# Launch Recurse directly into benchmark mode and capture a Tracy trace.
# Env: PROFILE_MODE    - "debug" (default) or "release"
#      CAPTURE_SECONDS  - seconds to capture (default: 30)
#      CAPTURE_PORT     - Tracy listen port (default: 8086)
set -eu

mode="${PROFILE_MODE:-debug}"
seconds="${CAPTURE_SECONDS:-30}"
port="${CAPTURE_PORT:-8086}"

case "$mode" in
    debug)   preset="dev-profile-debug" ;;
    release) preset="dev-profile-release" ;;
    *)
        echo "Unknown PROFILE_MODE: $mode" >&2
        exit 1
        ;;
esac

bin="build/${preset}/bin/Recurse"

if [ ! -x "$bin" ]; then
    echo "Binary not found: $bin" >&2
    echo "Run 'mise run profile' first." >&2
    exit 1
fi

# Resolve tracy-capture binary
tracy_capture=""
for candidate in tracy-capture /opt/homebrew/bin/tracy-capture; do
    if command -v "$candidate" >/dev/null 2>&1; then
        tracy_capture="$candidate"
        break
    fi
done
if [ -z "$tracy_capture" ]; then
    echo "tracy-capture not found. Install: brew install tracy" >&2
    exit 1
fi

mkdir -p tracy
timestamp="$(date +%Y-%m-%d-%H%M)"
outfile="tracy/${timestamp}.tracy"

echo "Starting Recurse (${preset}) in benchmark mode..."
"$bin" --benchmark &
app_pid=$!

# Give the app time to initialize Tracy listener
sleep 2

echo "Capturing trace to ${outfile} (${seconds}s, port ${port})..."
echo "Press Ctrl+C to stop early."

# tracy-capture connects, records, and writes the .tracy file on exit
"$tracy_capture" -o "$outfile" -s "$seconds" -p "$port" || true

echo "Stopping Recurse..."
kill "$app_pid" 2>/dev/null || true
wait "$app_pid" 2>/dev/null || true

if [ -f "$outfile" ]; then
    size="$(du -h "$outfile" | cut -f1)"
    echo "Trace saved: ${outfile} (${size})"
    echo "View with:   tracy-profiler ${outfile}"
    echo "Export CSV:  tracy-csvexport ${outfile}"
else
    echo "No trace file produced." >&2
    exit 1
fi
