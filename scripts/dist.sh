#!/bin/sh
# Assembles bin/ -- what goes on the memory card / USB -- from examples/.
#
#   examples/shell/      -> bin/              AthenaOS: main.js, main.lua, athenaos/, athena.ini
#   examples/shared/     -> bin/              assets several apps use (render/, fonts/, ...)
#   examples/<app>/      -> bin/apps/<app>/   app.json, main.js, main.lua, main.c (built into
#                                             athena_demos.elf; its presence tells the shells the
#                                             app has a C version)
#   examples/<app>/assets/ -> bin/            assets only that app uses, at the same paths as shared ones
#
# The ELFs and .erl modules are built straight into bin/ by make. Only files
# newer than the copy in bin/ are copied, so running it again is cheap. Seed
# files (settings, saves, athena.ini) are only copied when missing: bin/ keeps
# whatever the apps saved there.
#
# Usage: scripts/dist.sh [--clean]   --clean first removes everything in bin/
#                                    except the ELFs, the .erl modules and athena.ini
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
EX="$ROOT/examples"
BIN="$ROOT/bin"

if [ "$1" = "--clean" ]; then
    find "$BIN" -mindepth 1 -maxdepth 1 ! -name '*.elf' ! -name '*.erl' ! -name 'athena.ini' -exec rm -rf {} +
fi
mkdir -p "$BIN/apps"

is_seed() {
    case "$1" in
        athena.ini|settings.json|session.json|*_save.json|brew_data.json) return 0 ;;
    esac
    return 1
}

# sync SRC_DIR DST_DIR [PRUNE...]: every file under SRC_DIR into DST_DIR, at the
# same relative path, skipping the top-level entries named in PRUNE.
sync() {
    src_root=$1 dst_root=$2
    shift 2
    (cd "$src_root" && find . -type f) | while IFS= read -r rel; do
        rel=${rel#./}
        top=${rel%%/*}
        skip=0
        for p in "$@"; do [ "$top" = "$p" ] && skip=1; done
        [ $skip = 1 ] && continue
        src="$src_root/$rel" dst="$dst_root/$rel"
        if [ -e "$dst" ]; then
            is_seed "${rel##*/}" && continue
            [ "$src" -nt "$dst" ] || continue
        fi
        mkdir -p "${dst%/*}"
        cp -p "$src" "$dst"
    done
}

sync "$EX/shell" "$BIN"
sync "$EX/shared" "$BIN"

count=0
for dir in "$EX"/*/; do
    dir=${dir%/}
    [ -f "$dir/app.json" ] || continue
    name=$(basename "$dir")
    sync "$dir" "$BIN/apps/$name" assets
    [ -d "$dir/assets" ] && sync "$dir/assets" "$BIN"
    count=$((count + 1))
done

echo "dist: $count apps in $BIN"
