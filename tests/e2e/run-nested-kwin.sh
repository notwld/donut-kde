#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# End-to-end test of the real integration path, without touching the user's
# session: a nested `kwin_wayland --virtual` on a private D-Bus session bus,
# with HOME and every XDG directory redirected into a throw-away directory.
#
#   1. load the donutdragwatch effect into the nested KWin
#   2. start the Donut daemon (layer-shell overlay) inside it
#   3. a Qt client drags a real file (fake pointer input)
#   4. scenarios: drop on Zip, drop on Cancel, ordinary drag without pausing
#
# Usage: tests/e2e/run-nested-kwin.sh <build-dir>
set -euo pipefail

BUILD=$(realpath "${1:-build}")
SRC=$(cd "$(dirname "$0")" && pwd)
DRIVER="$BUILD/bin/donut-e2e-fakeinput"
DONUT="$BUILD/bin/donut"
for f in "$DRIVER" "$DONUT" "$BUILD/plugins/kwin/effects/plugins/donutdragwatch.so"; do
    [[ -e "$f" ]] || { echo "missing $f (build first)"; exit 2; }
done
command -v kwin_wayland >/dev/null || { echo "SKIP: kwin_wayland not installed"; exit 77; }

if [[ "${DONUT_E2E_INNER:-}" != 1 ]]; then
    WORK=$(mktemp -d "${TMPDIR:-/tmp}/donut-e2e.XXXXXX")
    trap 'rm -rf "$WORK"' EXIT
    DONUT_E2E_INNER=1 DONUT_E2E_WORK="$WORK" dbus-run-session -- "$0" "$BUILD"
    exit $?
fi
WORK=${DONUT_E2E_WORK}

# --- Isolation: nothing below may write to the real home ---------------------
export HOME="$WORK/home"
export XDG_CONFIG_HOME="$HOME/.config" XDG_DATA_HOME="$HOME/.local/share"
export XDG_CACHE_HOME="$HOME/.cache" XDG_STATE_HOME="$HOME/.local/state"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$XDG_STATE_HOME"
unset WAYLAND_DISPLAY DISPLAY
export QT_FORCE_STDERR_LOGGING=1 QT_MESSAGE_PATTERN="%{time process} %{category}: %{message}"
SOCKET="donut-e2e-$$"

cat > "$XDG_CONFIG_HOME/donutrc" <<EOF
[Trigger]
Mode=dwell
Threshold=16
DwellMs=400
[Appearance]
OpenDurationMs=120
BlurBehind=false
EOF

DRAG="$HOME/drag"
mkdir -p "$DRAG"
printf 'quarterly numbers\n' > "$DRAG/report q3 é.txt"
URI="file://$(python3 -c 'import sys,urllib.parse;print(urllib.parse.quote(sys.argv[1]))' "$DRAG/report q3 é.txt")"

QT_PLUGIN_PATH="$BUILD/plugins" QT_LOGGING_RULES="donut.*.debug=true" \
    kwin_wayland --virtual --socket "$SOCKET" --width 1280 --height 800 --no-lockscreen >"$WORK/kwin.log" 2>&1 &
KWIN=$!
for _ in $(seq 100); do [[ -S "$XDG_RUNTIME_DIR/$SOCKET" ]] && break; sleep 0.1; done
sleep 0.5
[[ "$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect donutdragwatch)" == true ]] || { echo "FAIL: effect did not load"; exit 1; }

export WAYLAND_DISPLAY="$SOCKET" QT_QPA_PLATFORM=wayland
QT_LOGGING_RULES="donut.*.debug=true" "$DONUT" >"$WORK/donut.log" 2>&1 &
DONUTPID=$!
sleep 1.5

run_drag() { # $1 = name, stdin = driver script
    /usr/lib/qt6/bin/qml "$SRC/source.qml" -- "$URI" >"$WORK/source-$1.log" 2>&1 &
    local src=$!
    sleep 1.5
    "$DRIVER"
    sleep 2
    kill "$src" 2>/dev/null || true
    wait "$src" 2>/dev/null || true
}

FAILS=0
check() { if eval "$2"; then echo "PASS: $1"; else echo "FAIL: $1"; FAILS=$((FAILS + 1)); fi; }
activations() { grep -c "donut.kwin: activating" "$WORK/kwin.log" || true; }

# Geometry: drag rests at (300,260) -> donut centred there, radius 170, hub 50.
# Sector centres at mid-radius 112: Zip (144°) = (366,351), Cancel (288°) = (193,225).

# --- 1. Pause mid-drag, drop on Zip ------------------------------------------
run_drag zip <<'IN'
move 200 200
sleep 1000
move 205 203
sleep 300
down
sleep 100
glide 300 260 12 240
sleep 800
glide 366 351 10 200
sleep 400
up
IN
check "drop on Zip creates the archive" '[[ -f "$DRAG/report q3 é.zip" ]]'
check "source file untouched" '[[ -f "$DRAG/report q3 é.txt" ]]'
check "source saw a Copy drop (never Move)" 'grep -q "drag finished, action 1" "$WORK/source-zip.log"'
check "exactly one activation" '[[ $(activations) == 1 ]]'

# --- 2. Pause mid-drag, release on Cancel ------------------------------------
before=$(ls "$DRAG" | wc -l)
run_drag cancel <<'IN'
move 200 200
sleep 300
down
sleep 100
glide 300 260 12 240
sleep 800
glide 193 225 10 200
sleep 400
up
IN
check "release on Cancel changes nothing" '[[ $(ls "$DRAG" | wc -l) == "$before" ]]'
check "source saw the drop refused" 'grep -q "drag finished, action 0" "$WORK/source-cancel.log"'
check "second drag activated once" '[[ $(activations) == 2 ]]'

# --- 3. Ordinary drag that never pauses: the donut must not appear -------------
run_drag ordinary <<'IN'
move 200 200
sleep 300
down
sleep 100
glide 700 500 30 600
up
IN
check "ordinary drag does not activate" '[[ $(activations) == 2 ]]'

kill "$DONUTPID" "$KWIN" 2>/dev/null || true
wait 2>/dev/null || true
if (( FAILS )); then
    echo "--- kwin.log (donut lines)"; grep donut "$WORK/kwin.log" || true
    echo "--- donut.log"; cat "$WORK/donut.log"
    exit 1
fi
echo "All end-to-end checks passed."
