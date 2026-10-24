#!/bin/sh
# record_games.sh RUNNER CLASSLIB OUTDIR GAME.jar...
#
# Runs each game for 40 s of emulated time from a fixed start time, with the
# same scripted key presses, and keeps per game a frame log (time and hash
# of every frame), the console output and the last screen. Two recordings
# of the same code are identical, so recording before and after a change
# and comparing them with
#   diff -r OUTDIR_BEFORE OUTDIR_AFTER
# shows whether the change altered anything a game does or shows. Used to
# check rewrites of the MIDP layer against real games.
#
# Git Bash on Windows: paths are converted with cygpath when it exists.
set -u
RUNNER=$1; LIB=$2; OUT=$3; shift 3
winpath() { if command -v cygpath >/dev/null 2>&1; then cygpath -w "$1"; else echo "$1"; fi; }

# Every 0.7 s a key press of 150 ms: fire, 5, down, fire, up, fire, left
# soft key, right, fire, left. Enough to get most games through their menus.
keys=""
t=2000
while [ $t -lt 40000 ]; do
    for k in -5 53 -2 -5 -1 -5 -6 -4 -5 -3; do
        keys="$keys$t:$k,$((t + 150)):$k:up,"
        t=$((t + 700))
    done
done
keys=${keys%,}

mkdir -p "$OUT"
for jar in "$@"; do
    name=$(basename "$jar" .jar)
    rm -rf "${OUT:?}/rms-${name:?}"
    "$RUNNER" --classlib "$(winpath "$LIB")" --epoch 0 --run-ms 40000 \
        --data "$(winpath "$OUT/rms-$name")" --keys "$keys" \
        --frame-log "$(winpath "$OUT/$name.frames")" --screenshot "$(winpath "$OUT/$name.ppm")" \
        "$(winpath "$jar")" > "$OUT/$name.out" 2>&1
    echo "$name: exit $?, $(wc -l < "$OUT/$name.frames") frames"
done
