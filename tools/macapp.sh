#!/bin/sh
# The macOS app of the releases: Refall.app (the standalone game: REFALL_STANDALONE=ON, the menu's music and the
# gamepad mappings built in, yt-dlp, ffmpeg and deno fetched on first use) in a disk image to drag to Applications.
# It is signed ad hoc, not by an Apple developer: the first time, macOS asks to confirm (right-click > Open, or System
# Settings > Privacy & Security > Open Anyway).
#
# usage: tools/macapp.sh BUILD_DIR OUT.dmg VERSION
#        (BUILD_DIR: a build configured with -DREFALL_STANDALONE=ON -DREFALL_BUNDLE_GLFW=ON, its refall target built)
# Needs Python with numpy and Pillow (the icon is drawn by tools/logo.py).
set -eu
build=$1
out=$2
version=$3
root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
app="$work/dmg/Refall.app" # the disk image holds the app and a link to Applications

mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
cp "$build/refall" "$app/Contents/MacOS/refall"
strip "$app/Contents/MacOS/refall" 2>/dev/null || true

# The icon: the window icon (the planet and its ring) at every size macOS asks for.
mkdir "$work/refall.iconset"
python3 - "$root" "$work/refall.iconset" <<'EOF'
import sys
sys.path.insert(0, sys.argv[1] + "/tools")
import logo
out = sys.argv[2]
for n in (16, 32, 128, 256, 512):
    logo.icon(n).save(f"{out}/icon_{n}x{n}.png")
    logo.icon(2 * n).save(f"{out}/icon_{n}x{n}@2x.png")
EOF
iconutil -c icns "$work/refall.iconset" -o "$app/Contents/Resources/refall.icns"

cat > "$app/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>Refall</string>
  <key>CFBundleDisplayName</key><string>Refall</string>
  <key>CFBundleIdentifier</key><string>com.github.gregwar.refall</string>
  <key>CFBundleExecutable</key><string>refall</string>
  <key>CFBundleIconFile</key><string>refall</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$version</string>
  <key>CFBundleVersion</key><string>$version</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.puzzle-games</string>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
  <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
EOF

codesign --force --deep --sign - "$app"
rm -f "$out"
ln -s /Applications "$work/dmg/Applications"
hdiutil create -volname Refall -srcfolder "$work/dmg" -ov -format UDZO "$out"
