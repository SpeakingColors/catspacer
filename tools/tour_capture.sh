#!/bin/bash
# Makes the interface-tour pictures from the REAL compiled plugin window (Linux, offscreen, no sound card needed).
# Usage: tools/tour_capture.sh "<path to the built Standalone binary>" <key> [preset name]
# Needs: xvfb, xdotool, imagemagick (import), python3 with pillow. Output goes to docs/tour/.
set -e
BIN="$1"; KEY="$2"; PRESET="${3:-Init}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"; OUT="$HERE/docs/tour"; TMP="$(mktemp -d)"
export HOME="$TMP/home"; mkdir -p "$HOME"
Xvfb :79 -screen 0 2600x1500x24 >/dev/null 2>&1 & XP=$!; sleep 2; export DISPLAY=:79
CAT_TOUR_DUMP="$TMP/dump.json" "$BIN" >"$TMP/app.log" 2>&1 & AP=$!; sleep 4
# enlarge the window to 2x so the pictures are sharp (the editor keeps its aspect ratio)
xdotool search --onlyvisible --class . 2>/dev/null | while read w; do xdotool windowsize "$w" 2010 1010 2>/dev/null; xdotool windowmove "$w" 0 0 2>/dev/null; done
sleep 5; xdotool mousemove 2590 1490; sleep 1.5
import -window root "$TMP/shot.png"
kill $AP $XP 2>/dev/null || true
python3 -I "$HERE/tools/tour_from_editor.py" "$KEY" "$TMP/shot.png" "$TMP/dump.json" "$OUT" "$PRESET"
rm -rf "$TMP"
