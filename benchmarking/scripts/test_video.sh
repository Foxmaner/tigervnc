#!/bin/bash
#
# Plays a webm video fullscreen in Xvnc with the video debugger overlay,
# which shows the detected video areas next to the ground truth zones
# from <video>.zones.
#
# Usage: test_video.sh <video.webm>
#

set -euo pipefail

# --- Setup -------------------------------------------------------------

REPO_DIR=$(cd "$(dirname "$0")/../.." && pwd)
XVNC=${XVNC:-$REPO_DIR/build/unix/xserver/hw/vnc/Xvnc}
VIDEO=${1:?Usage: $0 <video.webm>}
DISPLAY_NUM=${DISPLAY_NUM:-9}

# Match the display to the video size, so it plays unscaled and the
# ground truth zones line up with the screen
GEOMETRY=$(ffprobe -v error -select_streams v:0 \
  -show_entries stream=width,height -of csv=p=0:s=x "$VIDEO")

# Ground truth zones as "X1xY1:X2xY2,...", from GROUND_TRUTH or the
# .zones file next to the video
ZONES_FILE=${VIDEO%.*}.zones
if [ -z "${GROUND_TRUTH:-}" ]; then
  [ -f "$ZONES_FILE" ] || { echo "No ground truth, create $ZONES_FILE" >&2; exit 1; }
  GROUND_TRUTH=$(grep -v '^#' "$ZONES_FILE" | tr -d ' ' | grep . | paste -sd,)
fi

# --- Cleanup -----------------------------------------------------------

remove_x_files() {
  rm -f /tmp/.X11-unix/X$DISPLAY_NUM /tmp/.X$DISPLAY_NUM-lock
}

cleanup() {
  kill ${VIEWER_PID:-} ${MPV_PID:-} ${XVNC_PID:-} 2>/dev/null || true
  wait 2>/dev/null || true
  remove_x_files
}
trap cleanup EXIT

# Leftovers from a previous run
pkill -f "Xvnc :$DISPLAY_NUM" || true
sleep 0.5
remove_x_files

# --- Startup -----------------------------------------------------------

"$XVNC" :$DISPLAY_NUM \
  -geometry "$GEOMETRY" \
  -SecurityTypes None \
  -OverlayType video_debugger \
  -OverlayInput "$GROUND_TRUTH" \
  -OverlayPos tl \
  -OverlayPadding 0 \
  -OverlayAlpha 100 \
  >/dev/null 2>&1 &
XVNC_PID=$!

for _ in $(seq 50); do
  [ -S /tmp/.X11-unix/X$DISPLAY_NUM ] && break
  sleep 0.1
done

vncviewer localhost:$DISPLAY_NUM >/dev/null 2>&1 &
VIEWER_PID=$!
sleep 1

# --- Running -----------------------------------------------------------

DISPLAY=:$DISPLAY_NUM mpv --vo=x11 --fs --ao=null --no-osc --osd-level=0 \
  --no-input-default-bindings --cursor-autohide=always --really-quiet \
  "$VIDEO" &
MPV_PID=$!
wait "$MPV_PID" || true
MPV_PID=
