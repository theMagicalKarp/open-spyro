#!/usr/bin/env bash
# extract.sh — dump the original files from the source disc image into disc/orig/.
# Writes config/disc.xml (the repack layout descriptor) and asserts the SCUS_942.28 SHA-1.
# Originals stay gitignored (copyright); only disc.xml +
# sha1sums.txt are committed.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

SCUS_SHA1="84e3728ab94720d0873e2514adf4aade4935e0c5"

# dumpsxiso: prefer the copy built by `make setup` (tools/mkpsxiso/build), else $PATH.
DUMPSXISO="${DUMPSXISO:-}"
if [[ -z "$DUMPSXISO" ]]; then
  for c in \
    "$ROOT/tools/mkpsxiso/build/dumpsxiso" \
    "$(command -v dumpsxiso || true)"; do
    if [[ -n "$c" && -x "$c" ]]; then DUMPSXISO="$c"; break; fi
  done
fi
if [[ -z "$DUMPSXISO" || ! -x "$DUMPSXISO" ]]; then
  echo "extract: dumpsxiso not found. Run 'make setup-iso' to build it, or set DUMPSXISO=/path/to/dumpsxiso." >&2
  exit 1
fi

# Locate the source disc image in iso/ (a .cue is preferred; else a single .bin).
INPUT="${INPUT:-}"
if [[ -z "$INPUT" ]]; then
  shopt -s nullglob
  cues=(iso/*.cue)
  bins=(iso/*.bin)
  if [[ ${#cues[@]} -ge 1 ]]; then INPUT="${cues[0]}";
  elif [[ ${#bins[@]} -eq 1 ]]; then INPUT="${bins[0]}";
  fi
fi
if [[ -z "$INPUT" || ! -f "$INPUT" ]]; then
  echo "extract: no source disc found in iso/ (expected a .cue or .bin). Place it there (gitignored)." >&2
  exit 1
fi

echo "extract: dumping '$INPUT' -> disc/orig/ (xml: config/disc.xml)"
mkdir -p disc
"$DUMPSXISO" "$INPUT" -x disc/orig -s config/disc.xml

# Assert the SCUS SHA-1 — the matching target must come out byte-identical.
got="$(shasum disc/orig/SCUS_942.28 | awk '{print $1}')"
if [[ "$got" != "$SCUS_SHA1" ]]; then
  echo "extract: SCUS_942.28 SHA-1 mismatch!" >&2
  echo "  expected $SCUS_SHA1" >&2
  echo "  got      $got" >&2
  exit 1
fi
echo "extract: OK — disc/orig/SCUS_942.28 SHA-1 $got matches target."
