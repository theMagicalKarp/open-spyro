#!/usr/bin/env bash
# tools/build_main.sh — assemble + link the all-asm main EXE baseline.
#
# Runs INSIDE the matching Docker image (repo mounted at /work), e.g.
#   tools/docker_env.sh bash tools/build_main.sh
# (the Makefile `build` target wraps that). Produces build/main/SCUS_942.28,
# which `make verify` checks against disc/orig + the locked SHA-1.
#
# At this stage the whole EXE is original assembly reassembled (no C yet); each
# unmatched function will later be swapped to INCLUDE_ASM C in the match loop.
set -euo pipefail

AS="${AS:-mips-linux-gnu-as}"
LD="${LD:-mips-linux-gnu-ld}"
OBJCOPY="${OBJCOPY:-mips-linux-gnu-objcopy}"
OBJDUMP="${OBJDUMP:-mips-linux-gnu-objdump}"
# First-party build tooling is the `open-spyro` CLI (tools/open-spyro). It is
# installed in the matching image; PYTHONPATH points it at the mounted package
# source so repo edits stay authoritative for the link.
export PYTHONPATH="tools/open-spyro/src${PYTHONPATH:+:$PYTHONPATH}"
OPEN_SPYRO="${OPEN_SPYRO:-open-spyro}"

# -Iinclude resolves macro.inc / gte_macros.inc; explicit %gp_rel in the asm
# means -G0 is correct here.
AS_FLAGS="-EL -Iinclude -Iasm -march=r3000 -mtune=r3000 -no-pad-sections -G0 -msoft-float"

BUILD=build/main

# --- C overrides --------------------------------------------------------------
# Each src/c/<Name>.c replaces function <Name>'s asm. Two things must happen:
#   1. the asm body in asm/text.s is suppressed via `--defsym HAVE_C_<Name>=1`
#      (the .ifndef guard `open-spyro sectionize` wrapped each function in), so the
#      symbol isn't doubly defined and its slot is empty for the C to fill;
#   2. the .c is compiled to build/main/c/<Name>.o, which `open-spyro gen-slots-ld` has
#      already routed into <Name>'s fixed-VMA slot.
shopt -s nullglob
C_SRCS=(src/c/*.c)
shopt -u nullglob
HAVE_C_FLAGS=""
for c in "${C_SRCS[@]}"; do
  name="${c##*/}"; name="${name%.c}"   # bash builtins, not basename: a per-file
  HAVE_C_FLAGS+=" --defsym HAVE_C_${name}=1"   # subprocess costs ~8ms under x86 emulation
done

LD_SCRIPT=config/spyro.main.ld
SYMS_LD=config/spyro.main.syms.ld
ELF=$BUILD/spyro_main.elf
EXE=$BUILD/SCUS_942.28
EXE_SIZE=417792   # 0x66000 — exact original file length

# asm .s -> .o, mirroring the asm/ tree under build/main/ (paths the splat ld
# script references verbatim, e.g. build/main/asm/text.o).
asm_objs=(
  asm/header.s
  asm/text.s
  asm/data/rodata_pre.rodata.s
  asm/data/data.data.s
)

# Incremental rebuild: skip an object whose inputs are all older than it.
# Shared deps — the asm pulls include/*.inc; the C overrides pull include/*.h and
# compile through config/compile.sh. Newest dep file stands in for the whole set.
# asm/text.s and asm/data/rodata_pre.rodata.s additionally depend on the HAVE_C flag
# set (a src/c add/remove flips .ifndef guards), tracked via a per-object stamp file.
# `make clean` resets everything.
ASM_NEWEST="$(ls -t include/*.inc 2>/dev/null | head -1)"
C_NEWEST="$(ls -t config/compile.sh include/*.h 2>/dev/null | head -1)"

echo "build_main: assembling $(printf '%s ' "${asm_objs[@]}")"
for s in "${asm_objs[@]}"; do
  o="$BUILD/${s%.s}.o"
  stamp="${o%.o}.havec"
  mkdir -p "$(dirname "$o")"
  if [ -f "$o" ] && [ "$o" -nt "$s" ] && { [ -z "$ASM_NEWEST" ] || [ "$o" -nt "$ASM_NEWEST" ]; }; then
    case "$s" in
      asm/text.s|asm/data/rodata_pre.rodata.s)
        if [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$HAVE_C_FLAGS" ]; then continue; fi ;;
      *) continue ;;
    esac
  fi
  # HAVE_C_FLAGS only affects the files with .ifndef guards (text.s: every
  # function; rodata_pre: the jump tables); harmless on the others.
  "$AS" $AS_FLAGS $HAVE_C_FLAGS -o "$o" "$s"
  printf '%s' "$HAVE_C_FLAGS" > "$stamp"
done

# Compile each C override through the canonical cc1 -> maspsx -> as chain. Reject
# any object that emits a loadable .data/.sdata/.lit section, or a .rodata section
# when the function owns no rodata slot: the fixed-VMA layout only reserves a .text
# slot plus (for switch owners) the jump-table pieces in config/text_layout.json,
# so ld would orphan-place anything else into the globals RAM region (~0x80075640)
# where it corrupts live state -> hard-to-find hang. Shared strings/data are
# referenced by extern, never re-emitted.
RODATA_OWNERS=" $(python3 -c 'import json; print(" ".join(sorted({p["owner"] for p in json.load(open("config/text_layout.json")).get("rodata", []) if p.get("owner")})))') "
mkdir -p "$BUILD/c"   # constant dest dir: hoisted out of the loop (was a per-file mkdir spawn)
for c in "${C_SRCS[@]}"; do
  name="${c##*/}"; name="${name%.c}"   # bash builtins, not basename/dirname (emulated-subprocess cost)
  o="$BUILD/c/${name}.o"
  if [ -f "$o" ] && [ "$o" -nt "$c" ] && { [ -z "$C_NEWEST" ] || [ "$o" -nt "$C_NEWEST" ]; }; then
    continue  # up to date (the .rodata/.data guard ran when it was built)
  fi
  echo "build_main: compiling C override $c -> $o"
  bash config/compile.sh main "$c" "$o"
  bad_re='^\.(rodata|data|sdata|lit4|lit8)'
  case "$RODATA_OWNERS" in *" $name "*) bad_re='^\.(data|sdata|lit4|lit8)' ;; esac
  if "$OBJDUMP" -h "$o" | awk -v re="$bad_re" '$2 ~ re && $3 ~ /[1-9a-f]/ {bad=1} END {exit !bad}'; then
    echo "*** ERROR: $c emits a non-.text loadable section (ld would orphan-place it into globals RAM and corrupt state):" >&2
    "$OBJDUMP" -h "$o" | awk -v re="$bad_re" '$2 ~ re && $3 ~ /[1-9a-f]/ {print "    "$2" (size 0x"$3")"}' >&2
    echo "    Reference original ROM data by extern; only switch jump tables of a rodata-slot owner may be emitted." >&2
    rm -f "$o"; exit 1
  fi
done

# Pin every known symbol's address (.bss globals, kernel, hardware, BIOS) so the
# link resolves and every %hi/%lo/%gp_rel reproduces the original bytes.
$OPEN_SPYRO gen-syms-ld "$SYMS_LD" \
  config/symbol_addrs.txt \
  config/undefined_syms_auto.txt \
  config/undefined_funcs_auto.txt

# Per-function fixed-VMA text slots (config/spyro.main.slots.ld), anchoring every
# function at its address and routing each to its asm section or C override.
$OPEN_SPYRO gen-slots-ld

echo "build_main: linking $ELF"
"$LD" -EL --no-check-sections -T "$SYMS_LD" -T "$LD_SCRIPT" \
  -Map "$BUILD/spyro_main.map" -o "$ELF"

echo "build_main: objcopy -> $EXE"
"$OBJCOPY" -O binary "$ELF" "$EXE"

# Normalize to the exact original length: drop any trailing .bss zero-fill the
# objcopy may emit; zero-pad if somehow short. (The real image ends at 0x66000;
# .bss lives beyond it and is not part of the file.)
$OPEN_SPYRO normalize-binary "$EXE" "$EXE_SIZE"

echo "build_main: done -> $EXE"
