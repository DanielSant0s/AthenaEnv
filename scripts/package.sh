#!/bin/sh
# Release packages, from a built bin/ (make, make SCRIPT_ENGINE=lua, make dist).
#
#   js     AthenaEnv-js.zip     athena.elf + its modules + a hello world main.js
#   lua    AthenaEnv-lua.zip    athena_lua.elf + its modules + a hello world main.lua
#   demos  AthenaEnv-demos.zip  bin/ as make dist leaves it: AthenaOS, every app, both engines
#
# Each zip unpacks into an AthenaEnv/ folder.
#
# Usage: scripts/package.sh js|lua|demos... [-o OUTDIR]   (default OUTDIR: dist/)
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/bin"
OUT="$ROOT/dist"
TARGETS=""
while [ $# -gt 0 ]; do
    case "$1" in
        -o) OUT=$2; shift 2 ;;
        *) TARGETS="$TARGETS $1"; shift ;;
    esac
done
[ -n "$TARGETS" ] || { echo "usage: $0 js|lua|demos... [-o OUTDIR]" >&2; exit 1; }
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

need() {
    for f in "$@"; do
        [ -e "$BIN/$f" ] || { echo "package: bin/$f is missing -- build it first" >&2; exit 1; }
    done
}

# zip_stage NAME: zips $STAGE/AthenaEnv into OUT/NAME, then empties the stage.
zip_stage() {
    rm -f "$OUT/$1"
    (cd "$STAGE" && zip -qr9 "$OUT/$1" AthenaEnv)
    rm -rf "$STAGE/AthenaEnv"
    echo "package: $OUT/$1"
}

# starter ENGINE ELF_PREFIX MODULE_GLOB: an engine's ELFs, its .erl modules
# and the hello world from examples/starter/ENGINE.
starter() {
    engine=$1 prefix=$2
    need "$prefix.elf" "${prefix}_pkd.elf"
    mkdir -p "$STAGE/AthenaEnv"
    cp "$BIN/$prefix.elf" "$BIN/${prefix}_pkd.elf" "$STAGE/AthenaEnv/"
    for erl in "$BIN"/athena_*.erl; do
        [ -e "$erl" ] || continue
        case "$engine:$(basename "$erl")" in
            lua:*_lua.erl) cp "$erl" "$STAGE/AthenaEnv/" ;;
            js:*_lua.erl) ;;
            js:*) cp "$erl" "$STAGE/AthenaEnv/" ;;
        esac
    done
    cp "$ROOT/examples/starter/$engine/"* "$STAGE/AthenaEnv/"
    zip_stage "AthenaEnv-$engine.zip"
}

for target in $TARGETS; do
    case "$target" in
        js)  starter js athena ;;
        lua) starter lua athena_lua ;;
        demos)
            need athena.elf athena_lua.elf main.js main.lua apps
            mkdir -p "$STAGE/AthenaEnv"
            cp -R "$BIN/." "$STAGE/AthenaEnv/"
            # Local leftovers that are not part of a release.
            rm -f "$STAGE/AthenaEnv/athena_debug.elf" "$STAGE/AthenaEnv/athenaos/session.json" \
                  "$STAGE/AthenaEnv/brew_data.json" "$STAGE/AthenaEnv"/*_save.json
            cp "$ROOT/examples/shell/athena.ini" "$STAGE/AthenaEnv/athena.ini"
            zip_stage AthenaEnv-demos.zip
            ;;
        *) echo "package: unknown target '$target'" >&2; exit 1 ;;
    esac
done
