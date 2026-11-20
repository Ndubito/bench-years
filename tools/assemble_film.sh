#!/usr/bin/env bash
# Owner: A. Joins the final film from two sources with the same frame numbers:
#   out/frames/f_NNNN.ppm   every frame, from:  ./bench --frames out/frames/f 24 --json
#   out/blender/f_NNNN.png  scene frames rendered by Blender from out/frames/f_NNNN.json
# A frame with a Blender PNG uses it. A frame without one (the gust frames,
# or anything Blender has not rendered yet) uses the C++ image instead.
#
# usage:  bash tools/assemble_film.sh [frames_dir] [blender_dir] [output.mp4] [fps]
# needs:  ffmpeg on the PATH (MSYS2 UCRT64 window on Windows)
set -euo pipefail

FRAMES=${1:-out/frames}
BLENDER=${2:-out/blender}
OUTPUT=${3:-out/film.mp4}
FPS=${4:-24}
WORK=out/final

command -v ffmpeg >/dev/null || { echo "ffmpeg not found. Run this in the MSYS2 UCRT64 window."; exit 1; }
ls "$FRAMES"/f_*.ppm >/dev/null 2>&1 || { echo "no frames in $FRAMES. Run ./bench --frames $FRAMES/f $FPS --json first."; exit 1; }

rm -rf "$WORK" && mkdir -p "$WORK"
from_blender=0; from_cpp=0
for ppm in "$FRAMES"/f_*.ppm; do
    name=$(basename "$ppm" .ppm)                      # f_0123
    if [ -f "$BLENDER/$name.png" ]; then
        cp "$BLENDER/$name.png" "$WORK/$name.png"
        from_blender=$((from_blender + 1))
    else
        ffmpeg -y -loglevel error -i "$ppm" "$WORK/$name.png"
        from_cpp=$((from_cpp + 1))
    fi
done
echo "frames: $from_blender from Blender, $from_cpp from C++"

ffmpeg -y -loglevel error -framerate "$FPS" -i "$WORK/f_%04d.png" \
       -vf "scale=1280:720:flags=neighbor" -pix_fmt yuv420p "$OUTPUT"
echo "wrote $OUTPUT"
