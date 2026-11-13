#!/bin/bash
# Renders every variant of a universe off screen (xvfb) into one contact sheet.
# usage: tools/universesheet.sh UNIVERSE OUT.png [LEVEL 0|1|2 (default 1)] [SIZE (default 640x360)] [SONG]
set -e
cd "$(dirname "$0")/.."
U=${1:?universe}; OUT=${2:?output png}; L=${3:-1}; SIZE=${4:-640x360}
SONG=${5:-}
# a downloaded song when none is given
DEFAULT_SONG=$(ls ~/.cache/zentris/youtube/*.mp3 2>/dev/null | head -1)
TMP=$(mktemp -d)
mapfile -t VARS < <(./build/refall --list 2>/dev/null | grep -i "^  $U[^:]*:" | sed 's/^  //')
[ ${#VARS[@]} -gt 0 ] || { echo "no variants for $U"; exit 1; }
i=0
for v in "${VARS[@]}"; do
    ./build/refall --hidden --mute --size "$SIZE" --universe "$v" --review-shot "$TMP/$(printf %02d $i).png" \
        --review-level "$L" "${SONG:-$DEFAULT_SONG}" >/dev/null 2>&1 &
    i=$((i + 1))
done
wait
python3 - "$TMP" "$OUT" <<'PY'
import glob, sys
from PIL import Image
shots = [Image.open(p) for p in sorted(glob.glob(sys.argv[1] + "/*.png"))]
cols = min(4, len(shots)); rows = (len(shots) + cols - 1) // cols
w, h = shots[0].size
sheet = Image.new("RGB", (cols * (w + 4), rows * (h + 4)), "white")
for i, im in enumerate(shots):
    sheet.paste(im, ((i % cols) * (w + 4) + 2, (i // cols) * (h + 4) + 2))
sheet.save(sys.argv[2])
PY
rm -rf "$TMP"
echo "$OUT: $i variants of $U at level $L"
