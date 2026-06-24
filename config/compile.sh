#!/usr/bin/env bash
# config/compile.sh — the ONE canonical cc1 -> maspsx -> as -> ld command chain.
#
# This file is the source of truth for "how a translation unit is compiled to a
# byte-matching object". The Makefile sources these variables; do not
# fork the flags anywhere else. A casual flag change here can break every prior
# match, so treat edits like a toolchain bump.
#
# Toolchain (locked):
#   cc1      gcc 2.7.2, built from source into the Docker image (/opt/gcc2.7.2/cc1)
#   maspsx   wrapper that massages cc1 asm output into aspsx-compatible form
#   as/ld    GNU binutils for mips-linux-gnu (modern, from the Docker image)
#
# Usage (inside the matching environment, repo mounted at the CWD):
#   config/compile.sh main src/foo.c build/src/foo.o      # main EXE (-G8)
#   config/compile.sh ovl  src/bar.c build/src/bar.o      # overlay  (-G0)

set -euo pipefail

# --- tool locations (overridable; defaults match the Docker image layout) -----
CPP="${CPP:-mips-linux-gnu-cpp}"
CC1="${CC1:-/opt/gcc2.7.2/cc1}"
MASPSX="${MASPSX:-/opt/maspsx/maspsx.py}"
AS="${AS:-mips-linux-gnu-as}"
LD="${LD:-mips-linux-gnu-ld}"
OBJCOPY="${OBJCOPY:-mips-linux-gnu-objcopy}"
PYTHON="${PYTHON:-python3}"

# --- locked flags -------------------------------------------------------------
# Main EXE compiles with -G8; overlays override to -G0 (small-data threshold).
CC1_FLAGS="-O2 -G8 -g3 -fverbose-asm -mips1 -mcpu=3000 -fgnu-linker -mno-abicalls -mgpopt -msoft-float -quiet -funsigned-char"
CC1_FLAGS_OVL="-O2 -G0 -g3 -fverbose-asm -mips1 -mcpu=3000 -fgnu-linker -mno-abicalls -mgpopt -msoft-float -quiet -funsigned-char"

MASPSX_FLAGS="--aspsx-version 2.56 -G4 --expand-div"
MASPSX_FLAGS_OVL="--aspsx-version 2.56 -G0 --expand-div"

AS_FLAGS="-EL -Iinclude -Iasm -march=r3000 -mtune=r3000 -no-pad-sections -G0 -ggdb -msoft-float"

# cc1 (gcc 2.7.2) does NOT preprocess — it expects already-preprocessed input, so
# the chain must run cpp first (proven by the toolchain spike: without it any .c with
# a comment/#include/macro is a parse error). Modern target cpp from the Docker image.
CPP_FLAGS="-nostdinc -undef -Iinclude -Iasm"

# --- driver -------------------------------------------------------------------
# Allow `source config/compile.sh` to import the variables without running.
[ "${BASH_SOURCE[0]}" = "${0}" ] || return 0

mode="${1:?usage: compile.sh <main|ovl> <in.c> <out.o>}"
in_c="${2:?missing input .c}"
out_o="${3:?missing output .o}"

case "$mode" in
  main) cflags="$CC1_FLAGS";     mflags="$MASPSX_FLAGS" ;;
  ovl)  cflags="$CC1_FLAGS_OVL"; mflags="$MASPSX_FLAGS_OVL" ;;
  *) echo "compile.sh: mode must be 'main' or 'ovl', got '$mode'" >&2; exit 2 ;;
esac

tmp_i="$(mktemp /tmp/compile.XXXXXX.i)"
tmp_s="$(mktemp /tmp/compile.XXXXXX.s)"
trap 'rm -f "$tmp_i" "$tmp_s"' EXIT

# 1) cpp: preprocess (cc1 below does not — it needs preprocessed input)
"$CPP" $CPP_FLAGS "$in_c" -o "$tmp_i"
# 2) cc1: preprocessed C -> gcc 2.7.2 asm
"$CC1" $cflags "$tmp_i" -o "$tmp_s"
# 3) maspsx: gcc asm -> aspsx-compatible asm, piped straight into the assembler
"$PYTHON" "$MASPSX" $mflags < "$tmp_s" | "$AS" $AS_FLAGS -o "$out_o" -
