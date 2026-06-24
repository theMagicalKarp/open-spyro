#!/usr/bin/env bash
# tools/wad.sh — stage build/WAD.WAD from current parts.
#
# Produces build/WAD.WAD that mkpsxiso pulls in (config/disc.xml points WAD.WAD's
# source here, same override pattern as SCUS_942.28). Two modes:
#
#   * passthrough (default until any overlay is built from our sources): symlink
#     build/WAD.WAD -> the original disc/orig/WAD.WAD. No repack, no 110MB copy.
#   * repack: if build/overlays/*.ovl exist (overlays rebuilt from src/), run
#     `wadtool pack` with those rebuilt overlays substituted in (others fall back
#     to the originals) and write a fresh, byte-faithful build/WAD.WAD.
#
# The repack is proven byte-identical to disc/orig/WAD.WAD when no overlay differs
# (`open-spyro wad verify`), so swapping in a rebuilt-but-identical overlay still
# reproduces the original WAD bit-for-bit.
set -euo pipefail

# open-spyro CLI: the on-PATH copy inside the matching image, else the host
# tools/open-spyro uv project.
if command -v open-spyro >/dev/null 2>&1; then
  OPEN_SPYRO=(open-spyro)
else
  OPEN_SPYRO=(uv run --project tools/open-spyro -- open-spyro)
fi
ORIG_WAD=disc/orig/WAD.WAD
BUILD_OVL=build/overlays
OUT=build/WAD.WAD

[ -f "$ORIG_WAD" ]      || { echo "wad: $ORIG_WAD missing — run 'make extract'" >&2; exit 1; }
[ -f config/wad.json ]  || { echo "wad: config/wad.json missing — run 'open-spyro wad unpack'" >&2; exit 1; }

mkdir -p build
rm -f "$OUT"

if compgen -G "$BUILD_OVL/*.ovl" >/dev/null 2>&1; then
	n=$(ls "$BUILD_OVL"/*.ovl | wc -l | tr -d ' ')
	echo "wad: repacking with $n rebuilt overlay(s) from $BUILD_OVL/"
	"${OPEN_SPYRO[@]}" wad pack -o "$OUT" --overlay-dir "$BUILD_OVL"
else
	# Passthrough: no overlays built from source yet. Symlink the original.
	ln -s "$(cd "$(dirname "$ORIG_WAD")" && pwd)/$(basename "$ORIG_WAD")" "$OUT"
	echo "wad: passthrough — $OUT -> $ORIG_WAD (no rebuilt overlays)"
fi
