#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# User-local install of Donut into ~/.local (no root needed), plus enabling the
# donutdragwatch KWin effect. Re-run after every KWin upgrade: the effect only
# loads into the exact KWin version it was compiled against.
#
#   scripts/install.sh                 # everything in ~/.local; effect active after next login
#   scripts/install.sh --system-effect # also copy the effect .so to /usr/lib/qt6/plugins (sudo)
#                                      # so KWin can load it right now, no re-login
#   scripts/install.sh --no-start      # do not start the Donut daemon afterwards
#
# Uninstall with scripts/uninstall.sh. Nothing outside the listed paths is touched.
set -euo pipefail

SRC=$(cd "$(dirname "$0")/.." && pwd)
PREFIX="$HOME/.local"
BUILD="$SRC/build-install"
SYSTEM_EFFECT=0
START=1
for arg in "$@"; do
    case "$arg" in
    --system-effect) SYSTEM_EFFECT=1 ;;
    --no-start) START=0 ;;
    -h | --help) sed -n '3,15p' "$0"; exit 0 ;;
    *) echo "unknown option: $arg"; exit 2 ;;
    esac
done

STATE="$PREFIX/share/donut"
MANIFEST="$STATE/install-manifest.txt"
EFFECT_SO="donutdragwatch.so"
SYSTEM_EFFECT_DIR="/usr/lib/qt6/plugins/kwin/effects/plugins"
ENV_SCRIPT="$HOME/.config/plasma-workspace/env/donut-qt-plugin-path.sh"

echo "==> Building (Release)"
cmake -S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -DBUILD_TESTING=OFF >/dev/null
cmake --build "$BUILD"

echo "==> Installing into $PREFIX"
cmake --install "$BUILD" >/dev/null
mkdir -p "$STATE"
cp "$BUILD/install_manifest.txt" "$MANIFEST"

# The session's PATH may not contain ~/.local/bin: use absolute paths in launchers.
for f in "$PREFIX/share/applications/org.kde.donut.desktop" "$PREFIX/share/kio/servicemenus/donut-actions.desktop"; do
    sed -i "s|^Exec=donut|Exec=$PREFIX/bin/donut|" "$f"
done
update-desktop-database "$PREFIX/share/applications" 2>/dev/null || true

USER_EFFECT=$(grep "/$EFFECT_SO\$" "$MANIFEST" || true)
if (( SYSTEM_EFFECT )); then
    echo "==> Installing the KWin effect system-wide (sudo): $SYSTEM_EFFECT_DIR/$EFFECT_SO"
    sudo install -Dm644 "$USER_EFFECT" "$SYSTEM_EFFECT_DIR/$EFFECT_SO"
    echo "$SYSTEM_EFFECT_DIR/$EFFECT_SO" >"$STATE/system-effect.txt"
else
    # KWin is started by the session and only scans plugin paths known at that time.
    PLUGIN_ROOT=${USER_EFFECT%/kwin/effects/plugins/$EFFECT_SO}
    mkdir -p "$(dirname "$ENV_SCRIPT")"
    cat >"$ENV_SCRIPT" <<EOF
# Added by Donut's install.sh so KWin finds the user-local donutdragwatch effect.
export QT_PLUGIN_PATH="$PLUGIN_ROOT\${QT_PLUGIN_PATH:+:\$QT_PLUGIN_PATH}"
EOF
    echo "$ENV_SCRIPT" >>"$MANIFEST"
fi

echo "==> Enabling the effect in KWin"
kwriteconfig6 --file kwinrc --group Plugins --key donutdragwatchEnabled true
LOADED=$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect donutdragwatch 2>/dev/null || echo false)
if [[ "$LOADED" == true ]]; then
    echo "    effect loaded into the running KWin"
elif (( SYSTEM_EFFECT )); then
    echo "    WARNING: KWin did not load the effect; see docs/troubleshooting.md"
else
    echo "    effect will be active after you log out and back in once"
fi

if (( START )); then
    if pgrep -u "$USER" -x donut >/dev/null; then
        echo "==> Restarting Donut"
        pkill -u "$USER" -x donut || true
        sleep 0.5
    else
        echo "==> Starting Donut"
    fi
    setsid -f "$PREFIX/bin/donut" >/dev/null 2>&1 </dev/null
fi

cat <<EOF

Donut is installed.
  Status:     $PREFIX/bin/donut --diagnostics
  Settings:   $PREFIX/bin/donut --settings
  Fallback:   $PREFIX/bin/donut --window   (drop-target window, no effect needed)
  Shortcut:   Meta+Shift+D opens the donut at the pointer
  Try it:     drag a file in Dolphin, hold the pointer still for a moment,
              then release on a segment.
  Remove:     $SRC/scripts/uninstall.sh
EOF
