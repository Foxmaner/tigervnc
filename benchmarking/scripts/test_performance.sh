#!/bin/bash
#
# Plays a webm video fullscreen in Xvnc and samples the CPU and memory
# usage of the Xvnc process every 100 ms, the same interval as the
# measurements in test_video.sh. The logs and the results are saved in
# benchmarking/results/<test name>_perf_<date>_<time>/.
#
# Usage: test_performance.sh <video.webm> [test name]
#
# Extra options for Xvnc can be given in XVNC_ARGS, e.g.
# XVNC_ARGS="-Log *:stderr:30,VideoDetector:stderr:100"
#

set -euo pipefail

# --- Setup -------------------------------------------------------------

REPO_DIR=$(cd "$(dirname "$0")/../.." && pwd)
XVNC=${XVNC:-$REPO_DIR/build/unix/xserver/hw/vnc/Xvnc}
VIDEO=${1:?Usage: $0 <video.webm>}
TEST_NAME=${2:-$(basename "${VIDEO%.*}")}
DISPLAY_NUM=${DISPLAY_NUM:-9}

# Seconds between each sample, same as measurementInterval in
# OverlayContentVideoDebugger.cxx
SAMPLE_INTERVAL=0.1

RESULT_DIR=$REPO_DIR/benchmarking/results/${TEST_NAME}_perf_$(date +%Y%m%d_%H%M%S)
mkdir -p "$RESULT_DIR"

# Match the display to the video size, so it plays unscaled
GEOMETRY=$(ffprobe -v error -select_streams v:0 \
  -show_entries stream=width,height -of csv=p=0:s=x "$VIDEO")

#Grab clock tick per second
CLK_TCK=$(getconf CLK_TCK)
#Grab page size for memory
PAGE_SIZE=$(getconf PAGESIZE)

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
  ${XVNC_ARGS:-} \
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

# Raw samples of Xvnc while the video plays, as "time,cpu_ticks,rss_pages".
# The fields after the process name in /proc/<pid>/stat start at field 3,
# so utime (14) and stime (15) are at index 11 and 12, and rss (24) at 21
RAW_FILE=$RESULT_DIR/samples.raw
: >"$RAW_FILE"
while kill -0 "$MPV_PID" 2>/dev/null; do
  stat=$(</proc/$XVNC_PID/stat) 2>/dev/null || break
  read -ra fields <<<"${stat##*) }"
  echo "$EPOCHREALTIME,$((fields[11] + fields[12])),${fields[21]}" >>"$RAW_FILE"
  sleep $SAMPLE_INTERVAL
done

# Highest memory usage of Xvnc over the whole run
PEAK_RSS_KB=$(awk '/^VmHWM:/ { print $2 }' /proc/$XVNC_PID/status 2>/dev/null || echo 0)

wait "$MPV_PID" || true
MPV_PID=

# Stop everything so all logs are complete
cleanup
trap - EXIT

# --- Results -----------------------------------------------------------

# One row per sample. The CPU usage is the CPU time used since the
# previous sample divided by the time passed, where 100% is one core
echo "sample,time,cpu_percent,rss_mib" >"$RESULT_DIR/performance.csv"
awk -F, -v hz="$CLK_TCK" -v page="$PAGE_SIZE" '
  NR == 1 { start = $1 }
  NR > 1 && $1 > prev_time {
    printf "%d,%.3f,%.2f,%.2f\n", NR - 1, $1 - start,
           ($2 - prev_ticks) / hz / ($1 - prev_time) * 100,
           $3 * page / 1048576
  }
  { prev_time = $1; prev_ticks = $2 }' "$RAW_FILE" >>"$RESULT_DIR/performance.csv"

# Averages and maximums over all samples, and the total CPU time
SCORES=$(awk -F, -v hz="$CLK_TCK" '
  NR == FNR { if (FNR == 1) first = $2; last = $2; next }
  FNR > 1 {
    n++; cpu += $3; rss += $4
    if ($3 > cpu_max) cpu_max = $3
    if ($4 > rss_max) rss_max = $4
  }
  END {
    printf "samples=%d\n", n
    printf "cpu_avg_percent=%.2f\ncpu_max_percent=%.2f\n", n ? cpu / n : 0, cpu_max
    printf "cpu_time_s=%.2f\n", (last - first) / hz
    printf "rss_avg_mib=%.2f\nrss_max_mib=%.2f\n", n ? rss / n : 0, rss_max
  }' "$RAW_FILE" "$RESULT_DIR/performance.csv")
rm -f "$RAW_FILE"

{
  echo "test=$TEST_NAME"
  echo "video=$VIDEO"
  echo "video_size=$GEOMETRY"
  echo "duration=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$VIDEO")"
  echo "geometry=$GEOMETRY"
  echo "sample_interval_s=$SAMPLE_INTERVAL"
  echo "cores=$(nproc)"
  echo "xvnc_args=${XVNC_ARGS:-}"
  echo "commit=$(git -C "$REPO_DIR" describe --always --dirty)"
  echo "$SCORES"
  awk -v kb="${PEAK_RSS_KB:-0}" 'BEGIN { printf "rss_peak_mib=%.2f\n", kb / 1024 }'
} >"$RESULT_DIR/info.txt"

echo "Results saved in $RESULT_DIR"
cat "$RESULT_DIR/info.txt"
