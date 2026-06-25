#!/usr/bin/env bash
# tools/make_iso.sh — repack the rebuilt files into a runnable disc.
#
# Stages the original extracted tree (disc/orig) and overwrites SCUS_942.28 with
# the freshly built one (config/disc.xml pulls it via the file's `source` attr),
# then repacks MODE2/2352 via mkpsxiso into build/SpyrotheDragon.{bin,cue}.
# WAD.WAD is staged to build/WAD.WAD by tools/wad.sh: a passthrough
# symlink to the original until overlays are built from our sources, then a
# byte-faithful `open-spyro wad` repack. STR/S0/SOURCE pass through unchanged.
set -euo pipefail

MKPSXISO=tools/mkpsxiso/build/mkpsxiso
XML=config/disc.xml
BUILT=build/main/SCUS_942.28
OUT_BIN=build/SpyrotheDragon.bin
OUT_CUE=build/SpyrotheDragon.cue

[ -x "$MKPSXISO" ] || { echo "iso: $MKPSXISO missing — run 'make setup-iso' first" >&2; exit 1; }
[ -f "$BUILT" ]    || { echo "iso: $BUILT missing — run 'make build' first" >&2; exit 1; }
[ -d disc/orig ]   || { echo "iso: disc/orig missing — run 'make extract' first" >&2; exit 1; }

mkdir -p build
bash tools/wad.sh
"$MKPSXISO" -y -o "$OUT_BIN" -c "$OUT_CUE" "$XML"

echo "iso: wrote $OUT_BIN + $OUT_CUE"
