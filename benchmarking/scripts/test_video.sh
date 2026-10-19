#!/bin/bash
#
# Plays a webm video fullscreen in Xvnc with the video debugger overlay,
# which shows the detected video areas next to the ground truth zones
# from <video>.zones. The logs and the overlap results are saved in
# benchmarking/results/<test name>_<date>_<time>/.
#
# Usage: test_video.sh <video.webm> [test name]
#

set -euo pipefail

# --- Setup -------------------------------------------------------------

REPO_DIR=$(cd "$(dirname "$0")/../.." && pwd)
XVNC=${XVNC:-$REPO_DIR/build/unix/xserver/hw/vnc/Xvnc}
VIDEO=${1:?Usage: $0 <video.webm>}
TEST_NAME=${2:-$(basename "${VIDEO%.*}")}
DISPLAY_NUM=${DISPLAY_NUM:-9}

RESULT_DIR=$REPO_DIR/benchmarking/results/${TEST_NAME}_$(date +%Y%m%d_%H%M%S)
mkdir -p "$RESULT_DIR"

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
  >"$RESULT_DIR/Xvnc.log" 2>&1 &
XVNC_PID=$!

for _ in $(seq 50); do
  [ -S /tmp/.X11-unix/X$DISPLAY_NUM ] && break
  sleep 0.1
done

# Dont let client resize
vncviewer -RemoteResize=0 localhost:$DISPLAY_NUM >"$RESULT_DIR/viewer.log" 2>&1 &
VIEWER_PID=$!
sleep 1

# --- Running -----------------------------------------------------------

DISPLAY=:$DISPLAY_NUM mpv --vo=x11 --fs --ao=null --no-osc --osd-level=0 \
  --no-input-default-bindings --cursor-autohide=always \
  "$VIDEO" >"$RESULT_DIR/mpv.log" 2>&1 &
MPV_PID=$!
wait "$MPV_PID" || true
MPV_PID=

# Stop everything so all logs are complete before they are parsed
cleanup
trap - EXIT

# --- Results -----------------------------------------------------------

# One row per "OverlayContentVideoDebugger: Measurement: key=value ..."
# message with the pixel counts tp, tn, fp and fn. The scores are derived here:
# overlap (IoU) = tp/(tp+fp+fn), precision = tp/(tp+fp), recall = tp/(tp+fn)
#
# Log messages that span several lines have their continuation lines
# indented, so those are first joined onto the line they belong to
echo "sample,overlap,precision,recall,tp,tn,fp,fn" >"$RESULT_DIR/overlap.csv"
awk '/^   +/ { sub(/^ +/, " "); printf "%s", $0; next }
     NR > 1 { print "" }
     { printf "%s", $0 }
     END { print "" }' "$RESULT_DIR/Xvnc.log" |
  grep 'OverlayContentVideoDebugger: Measurement:' |
  awk '
    function ratio(num, den, empty) { return den ? num / den : empty }
    {
      delete v
      for (i = 1; i <= NF; i++)
        if (split($i, kv, "=") == 2)
          v[kv[1]] = kv[2]
      tp = v["tp"] + 0; tn = v["tn"] + 0; fp = v["fp"] + 0; fn = v["fn"] + 0
      # Nothing detected where there is nothing to detect is a perfect
      # match, but detecting nothing at all gives no precision
      printf "%d,%.4f,%.4f,%.4f,%d,%d,%d,%d\n", v["sample"],
             ratio(tp, tp + fp + fn, 1), ratio(tp, tp + fp, 0),
             ratio(tp, tp + fn, 1), tp, tn, fp, fn
    }' >>"$RESULT_DIR/overlap.csv" || true

# Share of samples with an overlap of at least each threshold, and the
# area under that success curve
awk -F, -v out="$RESULT_DIR/success.csv" '
  NR > 1 { overlap[n++] = $2 }
  END {
    print "threshold,success_rate" >out
    for (t = 0; t <= 20; t++) {
      hits = 0
      for (i = 0; i < n; i++)
        if (overlap[i] + 1e-9 >= t / 20)
          hits++
      rate = n ? hits / n : 0
      printf "%.2f,%.4f\n", t / 20, rate >out
      sum += rate
    }
    printf "samples=%d\nauc=%.4f\n", n, sum / 21
  }' "$RESULT_DIR/overlap.csv" >"$RESULT_DIR/scores.tmp"

{
  echo "test=$TEST_NAME"
  echo "video=$VIDEO"
  echo "video_size=$GEOMETRY"
  echo "duration=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$VIDEO")"
  echo "geometry=$GEOMETRY"
  echo "ground_truth=$GROUND_TRUTH"
  echo "commit=$(git -C "$REPO_DIR" describe --always --dirty)"
  cat "$RESULT_DIR/scores.tmp"
} >"$RESULT_DIR/info.txt"
rm -f "$RESULT_DIR/scores.tmp"

echo "Results saved in $RESULT_DIR"
cat "$RESULT_DIR/info.txt"
