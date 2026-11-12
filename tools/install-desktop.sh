#!/bin/sh
# Installs Refall's desktop entry and icon for the current user, so the taskbar, Alt+Tab and the app grid show the
# REFALL logo (Wayland compositors take the window's icon from the desktop entry matching its app id, "refall").
# Usage: tools/install-desktop.sh [path/to/refall]   (default: build/refall next to this script's repository)
set -e
here=$(cd "$(dirname "$0")/.." && pwd)
bin=${1:-$here/build/refall}
bin=$(cd "$(dirname "$bin")" && pwd)/$(basename "$bin")
share=${XDG_DATA_HOME:-$HOME/.local/share}
mkdir -p "$share/icons/hicolor/256x256/apps" "$share/applications"
cp "$here/data/icon.png" "$share/icons/hicolor/256x256/apps/refall.png"
cat > "$share/applications/refall.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Refall
Comment=A music-reactive falling-block puzzle game
Exec=$bin
Icon=refall
Terminal=false
Categories=Game;
StartupWMClass=refall
DESKTOP
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$share/applications" || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t "$share/icons/hicolor" || true
echo "installed $share/applications/refall.desktop (runs $bin) and the refall icon"
