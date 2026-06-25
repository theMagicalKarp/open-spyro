#!/usr/bin/env bash
# tools/verify_main.sh — check the rebuilt artifacts against the recorded SHA-1s.
# Asserts the full byte-identical set (main EXE + overlays):
#   * main EXE         build/main/SCUS_942.28   vs config/sha1sums.txt + disc/orig
#   * 37 overlays      build/overlays/<n>.ovl   vs config/overlays.json  + disc/orig
#   * WAD.WAD          repack from current parts is byte-identical (`open-spyro wad verify`)
#
# Two modes, auto-detected by whether disc/orig is present:
#   * FULL  (disc/orig present — local dev): byte-for-byte cmp AND SHA-1, plus the
#           WAD.WAD round-trip. This is the strongest check.
#   * SHA   (no disc/orig — CI): assert each rebuilt artifact's SHA-1 against the
#           recorded baselines (config/sha1sums.txt, config/overlays.json). No
#           copyrighted disc is ever needed. The WAD round-trip is skipped (it
#           reads original WAD parts from disc/orig); since the WAD is purely a
#           repack of these overlays + static data, all-overlays-match implies it.
# Exits non-zero on any mismatch (reports every failure before exiting). This is
# the regression gate: a newly-matched C function that doesn't reproduce bytes
# fails here.
set -euo pipefail

# open-spyro CLI: inside the matching image the console script is on PATH (no uv
# project there) — prefer it; on the host, run it from the tools/open-spyro uv
# project. PYTHONPATH points the image copy at the mounted package source so repo
# edits stay authoritative (harmless on the host).
export PYTHONPATH="tools/open-spyro/src${PYTHONPATH:+:$PYTHONPATH}"
if command -v open-spyro >/dev/null 2>&1; then
  OPEN_SPYRO=(open-spyro)
else
  OPEN_SPYRO=(uv run --project tools/open-spyro -- open-spyro)
fi
fail=0

sha() { shasum -a1 "$1" 2>/dev/null | awk '{print $1}'; }

# Mode: do we have the original disc files to byte-compare against?
if [ -d disc/orig ]; then
  HAVE_DISC=1
  echo "verify: FULL mode (disc/orig present) — byte-cmp + SHA-1 + WAD round-trip"
else
  HAVE_DISC=0
  echo "verify: SHA mode (no disc/orig) — SHA-1 of rebuilt artifacts vs recorded baselines"
fi

# ---- main EXE ----------------------------------------------------------------
BUILT=build/main/SCUS_942.28
ORIG=disc/orig/SCUS_942.28
# Single source of truth: the recorded baseline in config/sha1sums.txt.
WANT_SHA=$(awk '$2=="SCUS_942.28"{print $1}' config/sha1sums.txt)

if [ -z "$WANT_SHA" ]; then
  echo "verify: could not read SCUS_942.28 SHA-1 from config/sha1sums.txt" >&2
  fail=1
elif [ ! -f "$BUILT" ]; then
  echo "verify: $BUILT missing — run 'make build' first" >&2
  fail=1
else
  got=$(sha "$BUILT")
  if [ "$got" = "$WANT_SHA" ] && { [ "$HAVE_DISC" -eq 0 ] || cmp -s "$BUILT" "$ORIG"; }; then
    echo "verify: MATCH  SCUS_942.28 (sha1 $got)"
  else
    echo "verify: DIFFER SCUS_942.28 — built $got, want $WANT_SHA" >&2
    fail=1
  fi
fi

# ---- 37 overlays -------------------------------------------------------------
# SHA-1s frozen in config/overlays.json. Always assert SHA-1; in FULL mode also
# byte-compare against disc/orig. The command prints its own MATCH/DIFFER summary
# and exits non-zero on any mismatch.
compare=()
[ "$HAVE_DISC" -eq 1 ] && compare=(--compare-bytes)
"${OPEN_SPYRO[@]}" verify-overlays "${compare[@]}" || fail=1

# ---- WAD.WAD round-trip ------------------------------------------------------
# Proves the repack from current parts (incl. rebuilt overlays) is byte-identical.
# Needs the original WAD parts (disc/orig), so it is FULL-mode only. In SHA mode
# the all-overlays-match check above already covers the only bytes that can change.
if [ "$HAVE_DISC" -eq 1 ]; then
  if "${OPEN_SPYRO[@]}" wad verify >/dev/null 2>&1; then
    echo "verify: MATCH  WAD.WAD round-trip byte-identical"
  else
    echo "verify: DIFFER WAD.WAD repack is not byte-identical" >&2
    "${OPEN_SPYRO[@]}" wad verify >&2 || true
    fail=1
  fi
else
  echo "verify: SKIP   WAD.WAD round-trip (needs disc/orig parts; covered by overlay SHAs)"
fi

# ------------------------------------------------------------------------------
if [ "$fail" -eq 0 ]; then
  if [ "$HAVE_DISC" -eq 1 ]; then
    echo "verify: ALL byte-identical — main EXE + 37 overlays + WAD.WAD"
  else
    echo "verify: ALL SHA-1 match — main EXE + 37 overlays (WAD round-trip skipped)"
  fi
  exit 0
fi
echo "verify: FAILED — see mismatches above" >&2
exit 1
