#!/bin/sh
# The Linux AppImage of the releases: the standalone game (REFALL_STANDALONE=ON: the menu's music and the gamepad
# mappings built in, yt-dlp, ffmpeg and deno fetched on first use) in one file that runs on any recent distribution.
# It carries nothing else: the game needs only the system's OpenGL and X11, which must come from the system anyway.
#
# usage: tools/appimage.sh BUILD_DIR OUT.AppImage
#        (BUILD_DIR: a build configured with -DREFALL_STANDALONE=ON -DREFALL_BUNDLE_GLFW=ON, its refall target built)
# Needs appimagetool (downloaded next to the output when missing; set APPIMAGE_EXTRACT_AND_RUN=1 where FUSE is
# missing, e.g. in a container).
set -eu
build=$1
out=$2
root=$(cd "$(dirname "$0")/.." && pwd)
arch=$(uname -m)
app=$(mktemp -d)
chmod 755 "$app" # (appimagetool refuses a private folder)
trap 'rm -rf "$app"' EXIT

mkdir -p "$app/usr/bin"
cp "$build/refall" "$app/usr/bin/refall"
strip "$app/usr/bin/refall" 2>/dev/null || true
cp "$root/data/icon.png" "$app/refall.png"
cat > "$app/refall.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Refall
Comment=A falling-block puzzle game across many universes, staged by your music
Exec=refall
Icon=refall
Categories=Game;BlocksGame;
Terminal=false
EOF
cat > "$app/AppRun" <<'EOF'
#!/bin/sh
exec "$(dirname "$(readlink -f "$0")")/usr/bin/refall" "$@"
EOF
chmod +x "$app/AppRun"

tool=$(command -v appimagetool || true)
if [ -z "$tool" ]; then
  tool="$(dirname "$out")/appimagetool-$arch.AppImage"
  if [ ! -x "$tool" ]; then
    curl -L --fail -s -o "$tool" "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-$arch.AppImage"
    chmod +x "$tool"
  fi
fi
ARCH=$arch "$tool" --no-appstream "$app" "$out"
