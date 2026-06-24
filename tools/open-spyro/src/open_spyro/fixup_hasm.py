"""De-symbolize hand-asm functions so they relink byte-exact.

A handful of GTE-heavy render functions repurpose $gp as a scratch base pointer
(e.g. ``lui $gp,%hi(x); addiu $gp,$gp,%lo(x); addi $gp,$gp,0x1600`` then stream
``lw rt,off($gp)``). spimdisasm assumes $gp always holds the global pointer, so it
rewrites these immediates as %lo()/%gp_rel(SYM) that DON'T reassemble to the
original bytes (the +0x1600 becomes %lo(g_anActorDrawList)=0x12f4, etc).

These functions can't be symbolized correctly, so we emit their immediates raw.
Mark them in config/symbol_addrs.txt with ``can_reference:False``; this pass finds
those functions and replaces every %hi()/%lo()/%gp_rel(SYM) operand in their
address range with the literal immediate decoded from the instruction's own
ground-truth byte comment (``/* rom vram WORD */``). Raw immediates always
reassemble to the original bytes. Idempotent; re-run by ``make split``.
"""

from __future__ import annotations

import re

from open_spyro.paths import repo_root

# `/* <rom> <vram8> <WORD8> */  <mnemonic ...>`  — group1=vram, group2=file bytes.
_LINE = re.compile(r"/\*\s*[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s*\*/")
_RELOC = re.compile(r"%(?:hi|lo|gp_rel)\(([^)]*)\)")


def _load_ranges(syms_path) -> list[tuple[int, int]]:
    ranges: list[tuple[int, int]] = []
    for line in syms_path.read_text().splitlines():
        if "can_reference:False" not in line:
            continue
        m = re.match(r"\w+ = (0x[0-9A-Fa-f]+);.*size:(0x[0-9A-Fa-f]+)", line)
        if m:
            start = int(m.group(1), 16)
            ranges.append((start, start + int(m.group(2), 16)))
    return ranges


def _imm_str(word_le_bytes: bytes, unsigned: bool) -> str:
    """16-bit immediate (low halfword). lui takes an unsigned 0xNNNN; loads/addi
    take a signed offset (0xNN / -0xNN), matching splat's raw rendering."""
    imm = int.from_bytes(word_le_bytes, "little") & 0xFFFF
    if unsigned:
        return f"0x{imm:X}"
    if imm >= 0x8000:
        return f"-0x{0x10000 - imm:X}"
    return f"0x{imm:X}"


def run() -> None:
    repo = repo_root()
    asm = repo / "asm/text.s"
    syms = repo / "config/symbol_addrs.txt"

    ranges = _load_ranges(syms)
    if not ranges:
        print("fixup_hasm: no can_reference:False functions; nothing to do")
        return
    out: list[str] = []
    fixed = 0
    for line in asm.read_text().splitlines(keepends=True):
        m = _LINE.search(line)
        if m and _RELOC.search(line):
            vram = int(m.group(1), 16)
            if any(s <= vram < e for s, e in ranges):
                raw = _imm_str(
                    bytes.fromhex(m.group(2)), unsigned=bool(re.search(r"\blui\b", line))
                )
                line = _RELOC.sub(raw, line)
                fixed += 1
        out.append(line)
    asm.write_text("".join(out))
    print(
        f"fixup_hasm: rewrote {fixed} reloc operands to raw immediates "
        f"across {len(ranges)} hand-asm function(s)"
    )
