"""Cross-references from the disassembly: who reads / writes a data address, who calls a function.

Matched C hides most of the interesting accesses: the hand-written asm and the
held-base matches load a block base once (``lui``/``addiu``, often a raw
``0x8007`` immediate with no relocation) and then address its neighbours by
offset. ``xref`` tracks those base registers through ``asm/text.s`` and every
overlay's ``text.s`` and resolves each load / store to an absolute address, so a
global's readers and writers show up even when no symbol names them.

The tracking is a straight-line approximation (register values survive labels;
caller-saved registers die at ``jal``), good enough as a naming oracle: confirm
any surprising hit in the asm before relying on it.

    open-spyro xref data g_nSpyroCliffEdgeHint     # readers / writers by function
    open-spyro xref data 0x80077868 --size 0x20    # a raw range
    open-spyro xref callers WriteSpuRam            # jal callers (overlays included)
    open-spyro xref audit                          # unreferenced / write-only / never-stored

Besides loads (``R``) and stores (``W``) it reports address references
(``&``): an address formed by ``%lo`` / a raw ``lui`` + ``addiu`` pair, a
tracked address in an argument register at a ``jal`` (delay slot included), or
one stored as a value with ``sw``, or a ``.word`` pointer in a data table
(listed under the table's label). A ``base + index`` ``addu`` keeps the base, so
indexed table reads land on the table's first byte.
"""

from __future__ import annotations

import bisect
import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

from open_spyro.paths import repo_root

_INS = re.compile(r"/\*\s*\w+\s+([0-9A-F]{8})\s+\w+\s*\*/\s+(\S+)\s*(.*)$")
_CALLER_SAVED = frozenset("at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 t8 t9".split())
_ARG_REGS = ("a0", "a1", "a2", "a3")
_LOADS = frozenset("lw lh lhu lb lbu lwl lwr".split())
_STORES = frozenset("sw sh sb swl swr".split())
_NO_DEST = frozenset(
    "sw sh sb swl swr swc2 lwc2 beq bne beqz bnez bgez bgtz blez bltz bgezal bltzal "
    "j jr jal jalr mult multu div divu mtc0 mtc2 ctc2 mthi mtlo nop b break syscall".split()
)


@dataclass(frozen=True)
class Access:
    addr: int
    func: str
    op: str  # load / store mnemonic, or "&addr" / "&arg" / "&sw": address formed / passed / stored
    segment: str
    line: int


def _symbols(root: Path) -> tuple[dict[str, int], dict[int, str]]:
    """Name -> address for every seeded symbol, and address -> function name."""
    names: dict[str, int] = {}
    funcs: dict[int, str] = {}
    for line in (root / "config/symbol_addrs.txt").read_text().splitlines():
        m = re.match(r"\s*([A-Za-z_]\w*)\s*=\s*(0x[0-9a-fA-F]+);(.*)", line)
        if m:
            addr = int(m.group(2), 16)
            names.setdefault(m.group(1), addr)
            if "type:func" in m.group(3):
                funcs.setdefault(addr, m.group(1))
    return names, funcs


def _num(tok: str) -> int:
    neg = tok.startswith("-")
    tok = tok.lstrip("-")
    v = int(tok, 16) if tok.lower().startswith("0x") else int(tok)
    return -v if neg else v


def _sym_expr(expr: str, names: dict[str, int]) -> int | None:
    m = re.match(r"([A-Za-z_][\w.]*)\s*(?:([+-])\s*(\w+))?$", expr.strip())
    if not m:
        return None
    base = names.get(m.group(1))
    if base is None:
        auto = re.search(r"_([0-9A-Fa-f]{8})$", m.group(1))
        if not auto:
            return None
        base = int(auto.group(1), 16)
    if m.group(2):
        off = _num(m.group(3))
        base += off if m.group(2) == "+" else -off
    return base


def _asm_files(root: Path) -> list[tuple[str, Path]]:
    files = [("main", root / "asm/text.s")]
    files += [(p.parent.name, p) for p in sorted((root / "asm/overlays").glob("*/text.s"))]
    return files


def scan_accesses(root: Path) -> list[Access]:
    names, _ = _symbols(root)
    out: list[Access] = []
    for seg, path in _asm_files(root):
        regs: dict[str, int] = {}
        func = "?"
        idx = 0
        call_at: int | None = None  # index of a jal whose delay slot is still to run
        for ln, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
            if line.startswith("glabel"):
                func = line.split()[1]
                regs, call_at = {}, None
                continue
            m = _INS.search(line)
            if not m:
                continue
            idx += 1
            _step(m, regs, names, func, seg, ln, out)
            if call_at is not None:
                # The delay slot has run: record address arguments (every
                # tracked a-register; the previous call cleared stale ones),
                # then the callee clobbers the caller-saved registers.
                for r in _ARG_REGS:
                    if r in regs and 0x80010000 <= regs[r] < 0x80200000:
                        out.append(Access(regs[r], func, "&arg", seg, ln))
                for r in list(regs):
                    if r in _CALLER_SAVED:
                        regs.pop(r)
                call_at = None
            if m.group(2) in ("jal", "jalr", "bal"):
                call_at = idx
    out += _data_pointers(root, names)
    return out


_WORD = re.compile(r"/\*\s*\w+\s+([0-9A-F]{8})\s+\w+\s*\*/\s+\.word\s+(\S+)\s*$")


def _data_pointers(root: Path, names: dict[str, int]) -> list[Access]:
    """``.word`` pointers in data / rodata, attributed to the enclosing table ("&data")."""
    files = [("main", p) for p in sorted((root / "asm/data").glob("*.s"))]
    files += [(p.parent.parent.name, p) for p in sorted((root / "asm/overlays").glob("*/data/*.s"))]
    out: list[Access] = []
    for seg, path in files:
        table = "?"
        for ln, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
            if line.startswith("dlabel"):
                table = line.split()[1]
                continue
            m = _WORD.search(line)
            if not m or table.startswith("jtbl_") or m.group(2).startswith("."):
                continue
            tok = m.group(2)
            try:
                addr = _num(tok) if tok[0].isdigit() else _sym_expr(tok, names)
            except ValueError:
                addr = None
            if addr is not None and 0x80010000 <= addr < 0x80200000:
                out.append(Access(addr, table, "&data", seg, ln))
    return out


def _step(
    m: re.Match[str],
    regs: dict[str, int],
    names: dict[str, int],
    func: str,
    seg: str,
    ln: int,
    out: list[Access],
) -> None:
    """Apply one instruction to the tracked register values."""

    def setreg(r: str, v: int) -> None:
        regs[r] = v

    op = m.group(2)
    args_s = re.sub(r"/\*.*?\*/", "", m.group(3)).strip()
    args = [a.strip().lstrip("$") for a in re.split(r",(?![^(]*\))", args_s)] if args_s else []
    if op in _LOADS or op in _STORES:
        mm = re.match(r"(.*)\((\$?\w+)\)$", args[1]) if len(args) > 1 else None
        if mm:
            off, base = mm.group(1), mm.group(2).lstrip("$")
            addr = None
            rel = re.match(r"%(lo|gp_rel)\((.*)\)$", off)
            if rel:
                addr = _sym_expr(rel.group(2), names)
            elif base in regs:
                try:
                    addr = regs[base] + (_num(off) if off else 0)
                except ValueError:
                    addr = None
            if addr is not None and 0x80010000 <= addr < 0x80200000:
                out.append(Access(addr, func, op, seg, ln))
        if op == "sw" and args and args[0] in regs and 0x80010000 <= regs[args[0]] < 0x80200000:
            out.append(Access(regs[args[0]], func, "&sw", seg, ln))
        if op in _LOADS:
            regs.pop(args[0], None)
        return
    if op == "lui" and len(args) == 2:
        try:
            setreg(args[0], (_num(args[1]) & 0xFFFF) << 16)
        except ValueError:
            regs.pop(args[0], None)
        return
    if op in ("addiu", "addi") and len(args) == 3:
        lo = re.match(r"%lo\((.*)\)$", args[2])
        if lo:
            addr = _sym_expr(lo.group(1), names)
            if addr is not None:
                setreg(args[0], addr)
                out.append(Access(addr, func, "&addr", seg, ln))
                return
        if args[1] in regs:
            try:
                fresh = regs[args[1]] & 0xFFFF == 0  # completing a raw lui pair
                setreg(args[0], (regs[args[1]] + _num(args[2])) & 0xFFFFFFFF)
                if fresh and 0x80010000 <= regs[args[0]] < 0x80200000:
                    out.append(Access(regs[args[0]], func, "&addr", seg, ln))
                return
            except ValueError:
                pass
        regs.pop(args[0], None)
        return
    if (
        op in ("move", "or", "addu")
        and len(args) >= 2
        and args[1] in regs
        and (len(args) == 2 or args[2] == "zero")
    ):
        setreg(args[0], regs[args[1]])
        return
    if op in ("addu", "add") and len(args) == 3 and (args[1] in regs) != (args[2] in regs):
        # base + index: attribute later accesses to the table start.
        setreg(args[0], regs[args[1]] if args[1] in regs else regs[args[2]])
        return
    if op in ("jal", "jalr", "bal"):
        return
    if op not in _NO_DEST and args:
        regs.pop(args[0], None)


def call_graph(root: Path) -> dict[str, set[str]]:
    """Callee -> callers, from every ``jal`` in main and the overlays."""
    _, funcs = _symbols(root)
    for _seg, path in _asm_files(root)[:1]:
        cur = None
        pending = False
        for line in path.read_text(errors="replace").splitlines():
            m = re.match(r"glabel (\S+)", line)
            if m:
                cur, pending = m.group(1), True
                continue
            a = re.search(r"/\* \w+ ([0-9A-F]{8}) ", line)
            if a and cur and pending:
                funcs.setdefault(int(a.group(1), 16), cur)
                pending = False
    callers: dict[str, set[str]] = defaultdict(set)
    for _seg, path in _asm_files(root):
        cur = None
        for line in path.read_text(errors="replace").splitlines():
            if line.startswith("glabel"):
                cur = line.split()[1]
                continue
            m = re.search(r"\bjal\s+([A-Za-z_]\w*)\s*$", line.strip())
            if m and cur:
                target = m.group(1)
                auto = re.match(r"func_([0-9A-Fa-f]{8})$", target)
                if auto:
                    target = funcs.get(int(auto.group(1), 16), target)
                callers[target].add(cur)
    return callers


def data(target: str, size: int = 4) -> None:
    root = repo_root()
    names, _ = _symbols(root)
    if target.lower().startswith("0x"):
        base = int(target, 16)
    elif target in names:
        base = names[target]
    else:
        raise SystemExit(f"xref: unknown symbol {target}")
    hits: dict[int, dict[str, set[str]]] = defaultdict(lambda: defaultdict(set))
    for acc in scan_accesses(root):
        if base <= acc.addr < base + size:
            kind = "&" if acc.op.startswith("&") else "R" if acc.op.startswith("l") else "W"
            hits[acc.addr - base][kind].add(acc.func)
    if not hits:
        print(f"{target} ({base:#x}): no tracked access")
        return
    for off in sorted(hits):
        for kind in ("R", "W", "&"):
            if hits[off][kind]:
                print(f"+{off:#x} {kind}: {' '.join(sorted(hits[off][kind]))}")


def callers(func: str) -> None:
    graph = call_graph(repo_root())
    found = sorted(graph.get(func, ()))
    print(" ".join(found) if found else f"{func}: no jal callers (tables / tail jumps only?)")


_GLOBAL_DECL = re.compile(
    r"^extern\s+[^;]*?\b(g_\w+)\s*(?:\[[^\]]*\])*\s*;\s*/\*\s*(0x[0-9a-fA-F]{8})", re.M
)


_MAIN_BSS = 0x80075800  # main's .bss start (config/spyro.main.yaml)


def audit() -> None:
    """Flag ``include/globals.h`` globals nothing references, that are only ever written,
    or (in main's .bss) that are read but never stored, so stay 0 unless a block copy or
    CD read fills them.

    Each global covers its address up to the next seeded symbol (capped at 1 KiB),
    so a held-base read of a later field still counts. Hardware registers are skipped.
    """
    root = repo_root()
    names, _ = _symbols(root)
    starts = sorted({a for a in names.values() if 0x80010000 <= a < 0x80200000})
    kinds: dict[int, set[str]] = defaultdict(set)
    writers: dict[int, set[str]] = defaultdict(set)
    for acc in scan_accesses(root):
        # A bare "&addr" (an address formed, then loaded / stored through) is a
        # reference but not a read; passed / stored / tabled addresses may be.
        kind = (
            "A"
            if acc.op == "&addr"
            else "&"
            if acc.op.startswith("&")
            else "R"
            if acc.op.startswith("l")
            else "W"
        )
        kinds[acc.addr].add(kind)
        if kind == "W":
            writers[acc.addr].add(acc.func)
    hit = sorted(kinds)
    text = (root / "include/globals.h").read_text()
    for m in _GLOBAL_DECL.finditer(text):
        name, addr = m.group(1), int(m.group(2), 16)
        if not 0x80010000 <= addr < 0x80200000:
            continue
        i = bisect.bisect_right(starts, addr)
        end = min(starts[i] if i < len(starts) else addr + 4, addr + 0x400)
        lo, hi = bisect.bisect_left(hit, addr), bisect.bisect_left(hit, end)
        seen: set[str] = set()
        funcs: set[str] = set()
        for a in hit[lo:hi]:
            seen |= kinds[a]
            funcs |= writers[a]
        if not seen:
            print(f"unreferenced {addr:#x} {name}")
        elif seen <= {"W", "A"} and "W" in seen:
            print(f"write-only   {addr:#x} {name}: {' '.join(sorted(funcs))}")
        elif addr >= _MAIN_BSS and "R" in seen and not seen & {"W", "&"}:
            print(f"never-stored {addr:#x} {name}")
