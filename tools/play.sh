#!/usr/bin/env bash
# tools/play.sh — boot the rebuilt disc in DuckStation.
#
# Uses the macOS-reliable invocation: the binary directly with -batch. `open -a`
# does NOT boot the disc. Smoke path: BIOS -> Sony/Insomniac logos -> title ->
# "THE ADVENTURE BEGINS..." -> enter a level -> Spyro moves/charges/flames.
set -euo pipefail

DUCKSTATION="/Applications/DuckStation.app/Contents/MacOS/DuckStation"
CUE=build/SpyrotheDragon.cue

[ -x "$DUCKSTATION" ] || { echo "play: DuckStation not found at $DUCKSTATION" >&2; exit 1; }
[ -f "$CUE" ]         || { echo "play: $CUE missing — run 'make iso' first" >&2; exit 1; }

exec "$DUCKSTATION" -batch "$CUE"
