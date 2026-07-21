#!/usr/bin/env bash
# config/permuter_compile.sh — decomp-permuter compile shim.
#
# decomp-permuter's generated compile.sh calls the compiler as
#     <compiler_command tokens> "$INPUT" -o "$OUTPUT"
# (import.py finalize_compile_command), and its runtime invokes that wrapper as
#     compile.sh <in.c> -o <out.o>
# This bridges that convention to the repo's canonical
#     config/compile.sh <mode> <in.c> <out.o>
#
# The leading -I/-nostdinc flags baked into compiler_command are IGNORED here —
# they exist only so import.py's base.c preprocessing harvests them
# (import.py:580-596); config/compile.sh supplies its own include flags. Mode
# (main|ovl) comes from $SPYRO_PERM_MODE, exported by `open-spyro permuter`, so a
# single committed permuter_settings.toml serves both the main EXE and overlays.
set -euo pipefail

mode="${SPYRO_PERM_MODE:-main}"
in_c=""
out_o=""
while [ $# -gt 0 ]; do
  case "$1" in
    -o)  out_o="$2"; shift 2 ;;
    *.c) in_c="$1";  shift ;;
    *)   shift ;;
  esac
done

if [ -z "$in_c" ] || [ -z "$out_o" ]; then
  echo "permuter_compile.sh: could not parse input/output from args: $*" >&2
  exit 2
fi

exec "$(dirname "$0")/compile.sh" "$mode" "$in_c" "$out_o"
