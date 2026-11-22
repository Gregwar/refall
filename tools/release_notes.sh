#!/usr/bin/env bash
# A release's message: its section of CHANGELOG.md ("## 1.0.9" up to the next "## "), then how to install
# (.github/release-install.md). usage: tools/release_notes.sh v1.0.9 (or 1.0.9)
set -euo pipefail
cd "$(dirname "$0")/.."
v="${1#v}"
section=$(awk -v h="## $v" '$0 == h { on = 1; next } on && /^## / { exit } on' CHANGELOG.md | sed -e '/./,$!d')
[ -n "$section" ] || { echo "no section '## $v' in CHANGELOG.md" >&2; exit 1; }
printf '## What'"'"'s new\n\n%s\n\n' "$section"
cat .github/release-install.md
