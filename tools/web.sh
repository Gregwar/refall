#!/usr/bin/env bash
# The web version (Emscripten): the campaign only, its songs served with the page. Builds into build-web and makes
# the site in build-web/site: index.php, refall.js / .wasm / .data, and songs/<id>.mp3 for every song of
# data/campaign.tsv, taken from the game's song cache (<cache>/refall/youtube: play them once in the game, or pass
# another folder as SONGS=...). Needs the Emscripten SDK (EMSDK=~/emsdk by default).
# All static files: put the folder on any web server with PHP (index.php sends the headers its threads need; on Apache
# .htaccess does too), or on a static host (the page's service worker, sw.js, adds those headers then). Locally:
# php -S localhost:8000 -t build-web/site build-web/site/index.php, then http://localhost:8000 (other devices need HTTPS: browsers give threads
# only to secure pages).
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
rm -f "$site/index.html"
cp build-web/refall.js build-web/refall.wasm build-web/refall.data web/index.php web/.htaccess web/manifest.webmanifest "$site/"
rm -f "$site/coi-serviceworker.min.js"
cp data/icon.png "$site/icon.png"
# the app's icons (installed: 192 and 512 px, and a maskable one, the planet inside the circle Android may cut)
[ -f "$site/icon-maskable.png" ] && [ "$site/icon-maskable.png" -nt tools/logo.py ] ||
  python3 -c "import sys; sys.path.insert(0, 'tools'); import logo; from PIL import Image
logo.icon(192).save('$site/icon-192.png'); logo.icon(512).save('$site/icon-512.png')
m = Image.new('RGBA', (512, 512), (0, 0, 0, 255)); m.alpha_composite(logo.icon(368), (72, 72)); m.save('$site/icon-maskable.png')"
# the service worker (the threads' headers on static hosts, the game kept for offline play and installing), stamped
# with this build's files: a new build is a new version
version=$(cat build-web/refall.js build-web/refall.wasm build-web/refall.data web/index.php web/manifest.webmanifest | sha1sum | cut -c1-12)
sed "s/^const VERSION = \"dev\";/const VERSION = \"$version\";/" web/sw.js > "$site/sw.js"

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
