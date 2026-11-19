#!/usr/bin/env bash
# The web version (Emscripten): the campaign only, its songs served with the page. Builds into build-web and makes
# the site in build-web/site: index.html, refall.js / .wasm / .data, and songs/<id>.mp3 for every song of
# data/campaign.tsv, taken from the game's song cache (<cache>/refall/youtube: play them once in the game, or pass
# another folder as SONGS=...). Needs the Emscripten SDK (EMSDK=~/emsdk by default).
# Try it with: python3 tools/webserve.py   (then http://localhost:8000)
set -euo pipefail
cd "$(dirname "$0")/.."
EMSDK="${EMSDK:-$HOME/emsdk}"
SONGS="${SONGS:-${XDG_CACHE_HOME:-$HOME/.cache}/refall/youtube}"
# shellcheck disable=SC1091
source "$EMSDK/emsdk_env.sh" >/dev/null 2>&1
[ -f build-web/CMakeCache.txt ] || emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j"$(nproc 2>/dev/null || echo 4)"

site=build-web/site
mkdir -p "$site/songs"
cp build-web/refall.js build-web/refall.wasm build-web/refall.data web/index.html "$site/"
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
echo "site: $site ($(du -sh "$site" | cut -f1), $missing songs missing)"
