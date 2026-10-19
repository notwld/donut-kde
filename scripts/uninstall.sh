#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Removes exactly what scripts/install.sh installed (from its manifest), unloads
# and disables the KWin effect, and removes Donut's autostart entry and global
# shortcut registration. User settings (~/.config/donutrc) are kept unless
# --purge is given.
set -euo pipefail

PREFIX="$HOME/.local"
STATE="$PREFIX/share/donut"
MANIFEST="$STATE/install-manifest.txt"
PURGE=0
[[ "${1:-}" == --purge ]] && PURGE=1

echo "==> Stopping Donut"
pkill -u "$USER" -x donut 2>/dev/null || true

echo "==> Unloading and disabling the KWin effect"
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect donutdragwatch >/dev/null 2>&1 || true
kwriteconfig6 --file kwinrc --group Plugins --key donutdragwatchEnabled --delete

if [[ -f "$STATE/system-effect.txt" ]]; then
    SYS=$(cat "$STATE/system-effect.txt")
    if [[ "$SYS" == /usr/lib/qt6/plugins/kwin/effects/plugins/donutdragwatch.so && -f "$SYS" ]]; then
        echo "==> Removing system effect $SYS (sudo)"
        sudo rm -f "$SYS"
    fi
fi

if [[ -f "$MANIFEST" ]]; then
    echo "==> Removing installed files"
    while IFS= read -r f; do
        # Only ever delete files below the install prefix or Donut's env script.
        case "$f" in
        "$PREFIX"/* | "$HOME/.config/plasma-workspace/env/donut-qt-plugin-path.sh") rm -f -- "$f" ;;
        esac
    done <"$MANIFEST"
else
    echo "    no manifest at $MANIFEST; nothing to remove"
fi

rm -f "$HOME/.config/autostart/org.kde.donut.desktop"
# Forget the global shortcut component (harmless if it was never registered).
qdbus6 org.kde.kglobalaccel /component/donut org.kde.kglobalaccel.Component.cleanUp >/dev/null 2>&1 || true

if (( PURGE )); then
    rm -f "$HOME/.config/donutrc"
fi
rm -rf "$STATE"
update-desktop-database "$PREFIX/share/applications" 2>/dev/null || true
echo "Donut removed.$( ((PURGE)) || echo ' Settings kept in ~/.config/donutrc (use --purge to remove).')"
