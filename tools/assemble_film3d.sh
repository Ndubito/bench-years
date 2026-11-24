#!/usr/bin/env bash
# Joins all 3D Blender PNGs (scene frames + fade transition frames) into an MP4.
# usage:  bash tools/assemble_film3d.sh [blender_dir] [output.mp4] [fps]
set -euo pipefail

BLENDER=${1:-out/blender3d}
OUTPUT=${2:-out/film3d.mp4}
FPS=${3:-24}

command -v ffmpeg >/dev/null || { echo "ffmpeg not found. Run this in the MSYS2 UCRT64 window."; exit 1; }
ls "$BLENDER"/f_*.png >/dev/null 2>&1 || { echo "no frames in $BLENDER. Run blender -b -P blender/scene3d_render_numbered.py first."; exit 1; }

ffmpeg -y -loglevel error -framerate "$FPS" -i "$BLENDER/f_%04d.png" \
       -c:v libx264 -pix_fmt yuv420p "$OUTPUT"
echo "wrote $OUTPUT"