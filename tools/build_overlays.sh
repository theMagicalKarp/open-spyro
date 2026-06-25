#!/usr/bin/env bash
# tools/build_overlays.sh — assemble + link all 37 all-asm overlay baselines.
#
# The overlay equivalent of tools/build_main.sh. Runs INSIDE the matching Docker
# image (repo mounted at /work):
#   tools/docker_env.sh bash tools/build_overlays.sh
# Produces build/overlays/<name>.ovl for each overlay; `make verify` checks each
# against disc/orig/overlays/<name>.ovl and the SHA-1 frozen in config/overlays.json.
#
# Like the main EXE, every overlay is original assembly reassembled (no C yet).
# We link with splat's OWN generated <name>.ld (under build/overlays/, produced by
# `make split`): the overlay yaml labels the leading [id+.rodata] blob as `rodata`
# so splat orders it first and the link reproduces the file byte order. Using
# splat's ld (rather than a hand-rolled one) means it auto-updates as functions are
# split out to C. Externals (main-EXE funcs/globals) are PROVIDE()d from
# splat's per-overlay undefined_{syms,funcs}.auto.txt via the same `open-spyro
# gen-syms-ld` the main EXE uses.
set -euo pipefail

AS="${AS:-mips-linux-gnu-as}"
LD="${LD:-mips-linux-gnu-ld}"
OBJCOPY="${OBJCOPY:-mips-linux-gnu-objcopy}"
# First-party build tooling is the `open-spyro` CLI (installed in the matching
# image); PYTHONPATH points it at the mounted package source so repo edits stay
# authoritative for the link.
export PYTHONPATH="tools/open-spyro/src${PYTHONPATH:+:$PYTHONPATH}"
OPEN_SPYRO="${OPEN_SPYRO:-open-spyro}"

# Same flags as the main EXE / step-02 spike. -G0: overlay asm uses explicit
# %gp_rel; -Iinclude resolves macro.inc.
AS_FLAGS="-EL -Iinclude -Iasm -march=r3000 -mtune=r3000 -no-pad-sections -G0 -msoft-float"

OVL_DIR=build/overlays
ASM_DIR=asm/overlays

# Iterate overlays in config order. An optional arg ($1) filters to one overlay
# (substring match) for iterative debugging; no arg builds all 37.
FILTER="${1:-}"
mapfile -t names < <($OPEN_SPYRO list-overlays --filter "$FILTER")

echo "build_overlays: building ${#names[@]} overlays"
for name in "${names[@]}"; do
  src="$ASM_DIR/$name"
  # splat's generated ld references objects under build/overlays/<name>/asm/...
  # (build_path + the asm tree), so assemble into that mirrored layout.
  o_base="$OVL_DIR/$name/asm/overlays/$name"
  mkdir -p "$o_base/data"

  "$AS" $AS_FLAGS -o "$o_base/text.o"             "$src/text.s"
  "$AS" $AS_FLAGS -o "$o_base/data/rodata.rodata.o" "$src/data/rodata.rodata.s"
  "$AS" $AS_FLAGS -o "$o_base/data/data.data.o"   "$src/data/data.data.s"

  # PROVIDE() externals (main-EXE symbols this overlay calls) at fixed addresses.
  # splat's generated ld + undefined symbol lists are committed under config/overlays/
  # (see gen_overlay_yaml.py) so CI relinks from committed asm with no disc. The
  # PROVIDE() syms.ld is regenerated here each build, so it stays under build/.
  $OPEN_SPYRO gen-syms-ld "$OVL_DIR/$name.syms.ld" \
    "config/overlays/$name.undefined_syms.auto.txt" \
    "config/overlays/$name.undefined_funcs.auto.txt"

  # link (splat's ld + our externals) + objcopy to raw overlay image
  elf="$OVL_DIR/$name.elf"
  "$LD" -EL --no-check-sections \
    -T "$OVL_DIR/$name.syms.ld" -T "config/overlays/$name.ld" \
    -Map "$OVL_DIR/$name.map" -o "$elf"
  "$OBJCOPY" -O binary "$elf" "$OVL_DIR/$name.ovl"

  # Normalize to the exact original .ovl length (drop trailing .bss zero-fill;
  # zero-pad if short). Size is frozen in config/overlays.json.
  size=$($OPEN_SPYRO overlay-size "$name")
  $OPEN_SPYRO normalize-binary "$OVL_DIR/$name.ovl" "$size"
done

echo "build_overlays: done -> $OVL_DIR/*.ovl"
