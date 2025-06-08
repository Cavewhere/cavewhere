#!/usr/bin/env bash
set -euo pipefail

if (( $# != 1 )); then
  echo "Usage: $0 <source-png>"
  exit 1
fi

SRC="$1"
OUTDIR="Icons"
mkdir -p "$OUTDIR"

# name:pixelSize
icon_sizes=(
  "20:20"    "20@2x:40"   "20@3x:60"
  "29:29"    "29@2x:58"   "29@3x:87"
  "40:40"    "40@2x:80"   "40@3x:120"
  "60:60"    "60@2x:120"  "60@3x:180"
  "76:76"    "76@2x:152"
  "83.5@2x:167"
  "1024:1024"
)

for pair in "${icon_sizes[@]}"; do
  name=${pair%%:*}    # before the “:”
  px=${pair##*:}      # after the “:”
  magick "$SRC" -resize "${px}x${px}" \
    "$OUTDIR/AppIcon-${name}.png"
done
