#!/usr/bin/env bash
# The web version (Emscripten): the campaign only, its songs served with the page. Builds into build-web and makes
# the site in build-web/site: index.php, refall.js / .wasm / .data, and songs/<id>.mp3 for every song of
# data/campaign.tsv, taken from the game's song cache (<cache>/refall/youtube: play them once in the game, or pass
# another folder as SONGS=...); and worlds/*.jpg, the menu's universe snapshots from the game's cache
# (<cache>/refall/menu-worlds, made by the PC game's menu; WORLDS=... for another folder), which the browser would
# otherwise make on its main thread. Needs the Emscripten SDK (EMSDK=~/emsdk by default).
# All static files: put the folder on any web server with PHP (index.php sends the headers its threads need; on Apache
# .htaccess does too), or on a static host (a service worker in the page adds those headers then). Locally:
# php -S localhost:8000 -t build-web/site build-web/site/index.php, then http://localhost:8000 (other devices need HTTPS: browsers give threads
# only to secure pages).
set -euo pipefail
cd "$(dirname "$0")/.."
EMSDK="${EMSDK:-$HOME/emsdk}"
SONGS="${SONGS:-${XDG_CACHE_HOME:-$HOME/.cache}/refall/youtube}"
WORLDS="${WORLDS:-${XDG_CACHE_HOME:-$HOME/.cache}/refall/menu-worlds}"
# shellcheck disable=SC1091
source "$EMSDK/emsdk_env.sh" >/dev/null 2>&1
[ -f build-web/CMakeCache.txt ] || emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j"$(nproc 2>/dev/null || echo 4)"

site=build-web/site
mkdir -p "$site/songs"
rm -f "$site/index.html"
cp build-web/refall.js build-web/refall.wasm build-web/refall.data web/index.php web/.htaccess "$site/"
cp data/icon.png "$site/icon.png"
# Threads need a cross-origin isolated page: on hosts that can't send the headers (GitHub Pages), this service worker
# adds them (coi-serviceworker, MIT).
[ -f "$site/coi-serviceworker.min.js" ] ||
  curl -fsSL -o "$site/coi-serviceworker.min.js" https://cdn.jsdelivr.net/npm/coi-serviceworker@0.1.7/coi-serviceworker.min.js

missing=0
for id in $(grep -v '^#' data/campaign.tsv | awk -F'\t' 'NF > 3 { print $3 }'); do
  if [ -f "$SONGS/$id.mp3" ]; then
    cp -u "$SONGS/$id.mp3" "$site/songs/$id.mp3"
  else
    echo "missing song: $id (not in $SONGS)" >&2
    missing=$((missing + 1))
  fi
done
# the snapshots (raw RGB, 640x360) as JPEGs, and their list
mkdir -p "$site/worlds"
python3 - "$WORLDS" "$site/worlds" <<'PY'
import json, pathlib, sys
from PIL import Image
src, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
names = []
for f in sorted(src.glob("*-v2.rgb")):
    if f.stat().st_size != 640 * 360 * 3:
        continue
    n = f.name[: -len("-v2.rgb")]
    jpg = out / (n + ".jpg")
    if not jpg.exists() or jpg.stat().st_mtime < f.stat().st_mtime:
        Image.frombytes("RGB", (640, 360), f.read_bytes()).save(jpg, quality=88)
    names.append(n)
(out / "index.json").write_text(json.dumps(names))
print(f"worlds: {len(names)} snapshots")
PY
echo "site: $site ($(du -sh "$site" | cut -f1), $missing songs missing)"
