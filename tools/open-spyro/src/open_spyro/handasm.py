"""Literal R3000 -> C translator for the hand-written asm.

The 100 ``handwritten`` functions in ``progress.json`` cannot be matched from
C, so the native build translates them instruction by instruction. Under the
native memory model (ILP32, PS1 RAM identity-mapped) the translation is exact:
the same addresses, the same 32-bit arithmetic, the GTE through PsyZ's
register-level endpoints (``src/native/handasm/handasm.h``).

Model:

- Every register is a ``uint32_t`` local. Functions that reach each other by
  jumps, by ``jal`` or by name (``X`` / ``X_Tail``) form one **cluster**,
  emitted as one C function with a label per reachable address. Hand-written
  helpers pass values in arbitrary registers ("register-convention helper"),
  so the whole register file travels between clusters (``HaRegs``).
- ``jr reg`` dispatches through a switch over the cluster's code addresses:
  branch targets, ``jal`` return addresses, function entries and every
  instruction of a computed-jump run (``la base; add off; jr``; ``j`` tables
  and fixed-stride stub tables included). A target that is the cluster's
  entry ``ra`` returns; anything else traps with a log line.
- Branch and jump delay slots run before the transfer; a load whose result the
  next instruction reads gives that instruction the old value (R3000 load
  delay), as do ``mfc2`` / ``cfc2``.
- ``jal`` to game C calls the C function with the prototype from ``funcs.h``
  (its wasm signature); C callers reach a hand-asm function through a
  generated wrapper with that same signature.

Output: ``src/native/handasm/<cluster>.c`` plus ``handasm_index.h`` and
``functions.txt`` (the ``handwritten`` list, which stands in for
``progress.json`` where there is no rebuilt EXE to make it). The files
are generated; regenerate with ``open-spyro native handasm``. The literal
translation can be overridden per address range for native-only changes
(``handasm_patches.py``).
"""

from __future__ import annotations

import json
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

from open_spyro.handasm_patches import PATCHES, BranchPatch, Patch
from open_spyro.paths import repo_root

OUT_DIR = "src/native/handasm"
FUNCTIONS_TXT = "functions.txt"  # the hand-written functions translated, one per line
WASI_CLANG = "build/native-tools/wasi-sdk-34.0/bin/clang"

REG = (
    "zero at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 "
    "s0 s1 s2 s3 s4 s5 s6 s7 t8 t9 k0 k1 gp sp fp ra"
).split()

_GLABEL = re.compile(r"^glabel (\S+)")
_INSN = re.compile(r"^\s*/\* [0-9A-F]+ ([0-9A-F]{8}) ([0-9A-F]{8}) \*/")


# ---- input ------------------------------------------------------------------


@dataclass
class Func:
    name: str
    addr: int
    words: list[int] = field(default_factory=list)

    @property
    def end(self) -> int:
        return self.addr + 4 * len(self.words)


def read_text_functions(text_s: Path) -> dict[str, Func]:
    """Every glabel'd function in asm/text.s, by name, with its raw words."""
    funcs: dict[str, Func] = {}
    cur: Func | None = None
    for line in text_s.read_text().splitlines():
        m = _GLABEL.match(line)
        if m:
            cur = None
            name = m.group(1)
            if name not in funcs:
                cur = funcs[name] = Func(name, 0)
            continue
        # A function's last delay slot can sit after its endlabel line; it
        # ends at the next section / guard instead.
        if line.startswith((".section", ".endif", "nonmatching")):
            cur = None
            continue
        if cur is None:
            continue
        m = _INSN.match(line)
        if m:
            addr = int(m.group(1), 16)
            # splat prints the word as it is stored (little-endian bytes).
            word = struct.unpack("<I", bytes.fromhex(m.group(2)))[0]
            if not cur.words:
                cur.addr = addr
            elif addr != cur.end:
                raise SystemExit(f"handasm: {cur.name}: gap at {addr:08X}")
            cur.words.append(word)
    return {n: f for n, f in funcs.items() if f.words}


def handwritten_names(progress: Path) -> list[str]:
    fs = json.loads(progress.read_text())["functions"]
    return [f["name"] for f in fs if f.get("handwritten")]


# ---- wasm signatures of C prototypes ------------------------------------------


def _leb(b: bytes, i: int) -> tuple[int, int]:
    r = s = 0
    while True:
        x = b[i]
        i += 1
        r |= (x & 0x7F) << s
        s += 7
        if not x & 0x80:
            return r, i


_VT = {0x7F: "i32", 0x7E: "i64", 0x7D: "f32", 0x7C: "f64"}


def wasm_import_sigs(obj: bytes) -> dict[str, tuple[list[str], list[str]]]:
    """name -> (params, results) for every imported function of a wasm object."""
    assert obj[:4] == b"\0asm"
    i = 8
    types: list[tuple[list[str], list[str]]] = []
    sigs: dict[str, tuple[list[str], list[str]]] = {}
    while i < len(obj):
        sid = obj[i]
        size, i = _leb(obj, i + 1)
        body, i = obj[i : i + size], i + size
        j = 0
        if sid == 1:
            n, j = _leb(body, j)
            for _ in range(n):
                assert body[j] == 0x60
                np_, j = _leb(body, j + 1)
                ps = [_VT[x] for x in body[j : j + np_]]
                j += np_
                nr, j = _leb(body, j)
                rs = [_VT[x] for x in body[j : j + nr]]
                j += nr
                types.append((ps, rs))
        elif sid == 2:
            n, j = _leb(body, j)
            for _ in range(n):
                ml, j = _leb(body, j)
                j += ml
                fl, j = _leb(body, j)
                fname = body[j : j + fl].decode()
                j += fl
                kind = body[j]
                j += 1
                if kind == 0:
                    t, j = _leb(body, j)
                    sigs[fname] = types[t]
                elif kind == 1:
                    j += 1
                    flags, j = _leb(body, j)
                    _, j = _leb(body, j)
                    if flags & 1:
                        _, j = _leb(body, j)
                elif kind == 2:
                    flags, j = _leb(body, j)
                    _, j = _leb(body, j)
                    if flags & 1:
                        _, j = _leb(body, j)
                elif kind == 3:
                    j += 2
    return sigs


def c_signatures(names: list[str]) -> dict[str, tuple[list[str], list[str]]]:
    """wasm signature of each named function as funcs.h declares it.

    Compiles a TU that calls each one through its prototype (wasi-sdk clang,
    the game C flags) and reads the resulting imports, so the signature is
    exactly what a game C caller uses."""
    root = repo_root()
    src = ['#include "types.h"', '#include "funcs.h"', "void *handasm_sig_probe[] = {"]
    src += [f"  (void *){n}," for n in names]
    src.append("};")
    with tempfile.TemporaryDirectory() as td:
        c = Path(td) / "probe.c"
        o = Path(td) / "probe.o"
        c.write_text("\n".join(src) + "\n")
        subprocess.run(
            [
                str(root / WASI_CLANG),
                "--target=wasm32",
                "-c",
                "-O0",
                "-std=gnu89",
                "-nostdinc",
                "-funsigned-char",
                "-Wno-everything",
                "-DM2CTX",
                "-D__psyz",
                f"-I{root / 'include'}",
                f"-I{root / 'asm'}",
                str(c),
                "-o",
                str(o),
            ],
            check=True,
        )
        # Taking an address imports the function with its declared type.
        return wasm_import_sigs(o.read_bytes())


# ---- decode -----------------------------------------------------------------


@dataclass
class Insn:
    addr: int
    word: int
    c: str = ""  # the C statement (non-control instructions)
    reads: frozenset[int] = frozenset()
    writes: frozenset[int] = frozenset()
    load: int = -1  # destination register of a delayed load (lw.., mfc2, cfc2)
    kind: str = ""  # "", "branch", "j", "jal", "jr", "jalr"
    cond: str = ""
    target: int = 0
    reg: int = 0  # jr / jalr register


def s16(x: int) -> int:
    return x - 0x10000 if x & 0x8000 else x


def R(n: int) -> str:
    return "0u" if n == 0 else REG[n]


def decode(addr: int, w: int) -> Insn:  # noqa: C901 - one case per opcode
    op = w >> 26
    rs, rt, rd = (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
    sa, fn = (w >> 6) & 31, w & 63
    imm = w & 0xFFFF
    simm = s16(imm)
    i = Insn(addr, w)

    def setr(d: int, expr: str, *src: int) -> Insn:
        i.c = f"{REG[d]} = {expr};" if d else ""
        i.reads = frozenset(s for s in src if s)
        i.writes = frozenset([d]) if d else frozenset()
        return i

    def mem(load: str | None, store: str | None) -> Insn:
        ea = f"{R(rs)} + {simm}u" if simm >= 0 else f"{R(rs)} - {-simm}u"
        if load:
            i.load = rt
            return setr(rt, f"{load}({ea})", rs)
        i.c = f"{store}({ea}, {R(rt)});"
        i.reads = frozenset(s for s in (rs, rt) if s)
        return i

    def branch(cond: str, *src: int) -> Insn:
        i.kind = "branch"
        i.cond = cond
        i.target = (addr + 4 + (simm << 2)) & 0xFFFFFFFF
        i.reads = frozenset(s for s in src if s)
        return i

    S = lambda n: f"(int32_t){R(n)}"  # noqa: E731
    if w == 0:
        return i
    if op == 0:
        if fn == 0:
            return setr(rd, f"{R(rt)} << {sa}", rt)
        if fn == 2:
            return setr(rd, f"{R(rt)} >> {sa}", rt)
        if fn == 3:
            return setr(rd, f"(uint32_t)({S(rt)} >> {sa})", rt)
        if fn == 4:
            return setr(rd, f"{R(rt)} << ({R(rs)} & 31)", rt, rs)
        if fn == 6:
            return setr(rd, f"{R(rt)} >> ({R(rs)} & 31)", rt, rs)
        if fn == 7:
            return setr(rd, f"(uint32_t)({S(rt)} >> ({R(rs)} & 31))", rt, rs)
        if fn in (8, 9):
            i.kind = "jr" if fn == 8 else "jalr"
            i.reg = rs
            i.reads = frozenset([rs]) if rs else frozenset()
            if fn == 9:
                i.writes = frozenset([rd])
            return i
        if fn == 16:
            return setr(rd, "hi", 0)
        if fn == 18:
            return setr(rd, "lo", 0)
        if fn in (17, 19):
            i.c = f"{'hi' if fn == 17 else 'lo'} = {R(rs)};"
            i.reads = frozenset([rs]) if rs else frozenset()
            return i
        if fn in (24, 25, 26, 27):
            f = {24: "HA_MULT", 25: "HA_MULTU", 26: "HA_DIV", 27: "HA_DIVU"}[fn]
            i.c = f"{f}({R(rs)}, {R(rt)});"
            i.reads = frozenset(s for s in (rs, rt) if s)
            return i
        alu = {
            32: "{a} + {b}",
            33: "{a} + {b}",
            34: "{a} - {b}",
            35: "{a} - {b}",
            36: "{a} & {b}",
            37: "{a} | {b}",
            38: "{a} ^ {b}",
            39: "~({a} | {b})",
            42: "(uint32_t)((int32_t){a} < (int32_t){b})",
            43: "(uint32_t)({a} < {b})",
        }
        if fn in alu:
            return setr(rd, alu[fn].format(a=R(rs), b=R(rt)), rs, rt)
    elif op == 1:
        cond = {0: "{s} < 0", 1: "{s} >= 0"}.get(rt & 1)
        if rt in (0, 1, 16, 17):
            b = branch(str(cond).format(s=S(rs)), rs)
            if rt & 16:
                b.kind = "branch_link"
                b.writes = frozenset([31])
            return b
    elif op in (2, 3):
        i.kind = "j" if op == 2 else "jal"
        i.target = ((addr + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
        if op == 3:
            i.writes = frozenset([31])
        return i
    elif op in (4, 5):
        if op == 4 and rs == 0 and rt == 0:
            b = branch("1")
            b.kind = "j"
            return b
        return branch(f"{R(rs)} {'==' if op == 4 else '!='} {R(rt)}", rs, rt)
    elif op in (6, 7):
        return branch(f"{S(rs)} {'<=' if op == 6 else '>'} 0", rs)
    elif op in (8, 9):
        return setr(rt, f"{R(rs)} + {simm & 0xFFFFFFFF:#x}u", rs)
    elif op == 10:
        return setr(rt, f"(uint32_t)({S(rs)} < {simm})", rs)
    elif op == 11:
        return setr(rt, f"(uint32_t)({R(rs)} < {simm & 0xFFFFFFFF:#x}u)", rs)
    elif op == 12:
        return setr(rt, f"{R(rs)} & {imm:#x}u", rs)
    elif op == 13:
        return setr(rt, f"{R(rs)} | {imm:#x}u", rs)
    elif op == 14:
        return setr(rt, f"{R(rs)} ^ {imm:#x}u", rs)
    elif op == 15:
        return setr(rt, f"{imm << 16:#x}u")
    elif op == 18:
        if w & (1 << 25):
            i.c = f"HA_GTE_CMD({w & 0x1FFFFFF:#x});"
            return i
        if rs == 0:  # mfc2
            i.load = rt
            return setr(rt, f"HA_MFC2({rd})")
        if rs == 2:  # cfc2
            i.load = rt
            return setr(rt, f"HA_CFC2({rd})")
        if rs in (4, 6):
            i.c = f"HA_{'MTC2' if rs == 4 else 'CTC2'}({rd}, {R(rt)});"
            i.reads = frozenset([rt]) if rt else frozenset()
            return i
    elif op == 32:
        return mem("HA_LB", None)
    elif op == 33:
        return mem("HA_LH", None)
    elif op == 35:
        return mem("HA_LW", None)
    elif op == 36:
        return mem("HA_LBU", None)
    elif op == 37:
        return mem("HA_LHU", None)
    elif op in (34, 38):  # lwl / lwr merge into rt: rt is also a source
        ea = f"{R(rs)} + {simm & 0xFFFFFFFF:#x}u"
        f = "HA_LWL" if op == 34 else "HA_LWR"
        i.load = rt
        setr(rt, f"{f}({ea}, {R(rt)})", rs, rt)
        return i
    elif op == 40:
        return mem(None, "HA_SB")
    elif op == 41:
        return mem(None, "HA_SH")
    elif op == 43:
        return mem(None, "HA_SW")
    elif op in (42, 46):
        ea = f"{R(rs)} + {simm & 0xFFFFFFFF:#x}u"
        i.c = f"{'HA_SWL' if op == 42 else 'HA_SWR'}({ea}, {R(rt)});"
        i.reads = frozenset(s for s in (rs, rt) if s)
        return i
    elif op == 50:  # lwc2
        ea = f"{R(rs)} + {simm & 0xFFFFFFFF:#x}u"
        i.c = f"HA_MTC2({rt}, HA_LW({ea}));"
        i.reads = frozenset([rs]) if rs else frozenset()
        return i
    elif op == 58:  # swc2
        ea = f"{R(rs)} + {simm & 0xFFFFFFFF:#x}u"
        i.c = f"HA_SW({ea}, HA_MFC2({rt}));"
        i.reads = frozenset([rs]) if rs else frozenset()
        return i
    raise SystemExit(f"handasm: unsupported instruction {w:08X} at {addr:08X}")


# ---- clusters -----------------------------------------------------------------


class UF:
    def __init__(self) -> None:
        self.p: dict[str, str] = {}

    def find(self, x: str) -> str:
        self.p.setdefault(x, x)
        while self.p[x] != x:
            self.p[x] = self.p[self.p[x]]
            x = self.p[x]
        return x

    def union(self, a: str, b: str) -> None:
        self.p[self.find(a)] = self.find(b)


@dataclass
class Cluster:
    name: str
    funcs: list[Func]
    insns: dict[int, Insn] = field(default_factory=dict)
    labels: set[int] = field(default_factory=set)
    dispatch: set[int] = field(default_factory=set)
    patches: list[Patch | BranchPatch] = field(default_factory=list)

    def owns(self, a: int) -> bool:
        return a in self.insns


def build_clusters(hw: list[Func], all_funcs: dict[str, Func]) -> list[Cluster]:
    """Group the hand-asm functions into clusters.

    Functions the hand-asm jumps or falls into (small register-convention
    tails such as ReturnT0Fixed12, matched in C but not callable from asm the
    way asm reaches them) are pulled in and translated from their asm too."""
    by_start = sorted(all_funcs.values(), key=lambda f: f.addr)
    members = {f.name: f for f in hw}

    def owner(a: int) -> Func | None:
        for f in by_start:
            if f.addr <= a < f.end:
                return f
        return None

    uf = UF()
    work = list(hw)
    while work:
        f = work.pop()
        uf.find(f.name)
        reached: list[Func] = []
        if f.name.endswith("_Tail") and f.name[:-5] in members:
            reached.append(members[f.name[:-5]])
        last = decode(f.end - 8, f.words[-2]) if len(f.words) > 1 else None
        if last is None or last.kind not in ("j", "jr"):
            nxt = owner(f.end)  # falls off its end into the next function
            if nxt:
                reached.append(nxt)
        for k, w in enumerate(f.words):
            ins = decode(f.addr + 4 * k, w)
            if ins.kind in ("branch", "branch_link", "j"):
                o = owner(ins.target)
                if o and o is not f:
                    reached.append(o)
            elif ins.kind == "jal":
                o = owner(ins.target)
                if o and o.name in members and o is not f:
                    reached.append(o)
        for o in reached:
            if o.name not in members:
                members[o.name] = o
                work.append(o)
            uf.union(f.name, o.name)
    groups: dict[str, list[Func]] = defaultdict(list)
    for f in members.values():
        groups[uf.find(f.name)].append(f)
    clusters = []
    for fs in groups.values():
        fs.sort(key=lambda f: f.addr)
        cl = Cluster(fs[0].name, fs)
        for f in fs:
            for k, w in enumerate(f.words):
                a = f.addr + 4 * k
                cl.insns[a] = decode(a, w)
        clusters.append(cl)
    clusters.sort(key=lambda c: c.funcs[0].addr)
    return clusters


def find_labels(cl: Cluster) -> None:
    """Static targets get labels; computed-jump candidates also get a dispatch case."""
    addrs = sorted(cl.insns)
    for f in cl.funcs:
        cl.labels.add(f.addr)
        cl.dispatch.add(f.addr)
    for a in addrs:
        ins = cl.insns[a]
        if ins.kind in ("branch", "branch_link", "j", "jal") and cl.owns(ins.target):
            cl.labels.add(ins.target)
        if ins.kind in ("jal", "jalr", "branch_link"):
            cl.labels.add(a + 8)
            cl.dispatch.add(a + 8)
    # Code addresses built as constants (lui + addiu/ori): computed jumps.
    for k, a in enumerate(addrs):
        w = cl.insns[a].word
        if w >> 26 != 15:
            continue
        rt, hi = (w >> 16) & 31, (w & 0xFFFF) << 16
        for b in addrs[k + 1 : k + 4]:
            w2 = cl.insns[b].word
            if w2 >> 26 in (9, 13) and (w2 >> 21) & 31 == rt:
                lo = w2 & 0xFFFF
                val = (hi + (s16(lo) if w2 >> 26 == 9 else lo)) & 0xFFFFFFFF
                if cl.owns(val):
                    # Every instruction of the run, up to its first
                    # unconditional transfer (and that transfer's delay slot).
                    # Two table shapes go on past that transfer: a `j` table
                    # (the run starts with `j`; every entry is a `j`, or a
                    # last `jr`, + delay slot) and a stub table (each
                    # fixed-stride stub ends in `j join`; the run goes on up
                    # to the join).
                    c, end = val, None
                    jtable = cl.insns[val].kind == "j"
                    while cl.owns(c) and (end is None or c < end):
                        cl.labels.add(c)
                        cl.dispatch.add(c)
                        ins = cl.insns[c]
                        if ins.kind not in ("j", "jr"):
                            c += 4
                            continue
                        if cl.owns(c + 4):
                            cl.labels.add(c + 4)
                            cl.dispatch.add(c + 4)
                        nxt = cl.insns.get(c + 8)
                        if jtable:
                            if ins.kind != "j" or nxt is None or nxt.kind not in ("j", "jr"):
                                break
                        elif ins.kind == "j" and ins.target > c + 4:
                            end = ins.target if end is None else min(end, ins.target)
                        else:
                            break
                        c += 8
                break


# ---- patches ------------------------------------------------------------------

_GOTO = re.compile(r"\bL_([0-9A-F]{8})\b")


def check_patches(cl: Cluster) -> tuple[dict[int, Patch], dict[int, BranchPatch]]:
    """CL's range patches by start address and branch patches by address,
    after the rules in handasm_patches.py (labels must be known: run after
    find_labels)."""

    def fail(p: Patch, msg: str) -> SystemExit:
        return SystemExit(f"handasm: patch {p.func} {p.start:08X}..{p.end:08X}: {msg}")

    by_start: dict[int, Patch] = {}
    branches: dict[int, BranchPatch] = {}
    covered: set[int] = set()
    for b in cl.patches:
        if not isinstance(b, BranchPatch):
            continue
        ins = cl.insns.get(b.addr)
        where = f"handasm: branch patch {b.func} {b.addr:08X}"
        if ins is None:
            raise SystemExit(f"{where}: not in cluster {cl.name}")
        if ins.word != b.expect:
            raise SystemExit(f"{where}: stale: the asm there is now {ins.word:08X}")
        if ins.kind not in ("branch", "branch_link"):
            raise SystemExit(f"{where}: not a conditional branch")
        if b.addr in branches:
            raise SystemExit(f"{where}: patched twice")
        branches[b.addr] = b
    for p in cl.patches:
        if isinstance(p, BranchPatch):
            continue
        if p.start % 4 or p.end % 4 or p.end <= p.start:
            raise fail(p, "not a range of whole instructions")
        span = range(p.start, p.end, 4)
        missing = [a for a in span if a not in cl.insns]
        if missing:
            raise fail(p, f"{missing[0]:08X} is not in cluster {cl.name}")
        words = tuple(cl.insns[a].word for a in span)
        if words != p.expect:
            got = " ".join(f"{w:08X}" for w in words)
            raise fail(p, f"stale: the asm there is now {got}")
        if covered & set(span):
            raise fail(p, "overlaps another patch")
        covered |= set(span)
        before, last = cl.insns.get(p.start - 4), cl.insns[p.end - 4]
        if before is not None and before.kind:
            raise fail(p, f"starts in the delay slot of {before.addr:08X}")
        if last.kind:
            raise fail(p, f"ends between {last.addr:08X} and its delay slot")
        inner = [a for a in span[1:] if a in cl.labels]
        if inner:
            raise fail(p, f"{inner[0]:08X} inside it is a jump target")
        first, after = cl.insns[p.start], cl.insns.get(p.end)
        if before is not None and before.load > 0 and before.load in first.reads:
            raise fail(p, f"load-delay hazard into its start from {before.addr:08X}")
        if after is not None and last.load > 0 and last.load in after.reads:
            raise fail(p, f"load-delay hazard out of its end into {p.end:08X}")
        for m in _GOTO.finditer(p.c):
            t = int(m.group(1), 16)
            if t not in cl.labels:
                raise fail(p, f"goto {m.group(0)}: not a label of {cl.name}")
            prev = cl.insns.get(t - 4)
            if prev is not None and prev.kind:
                raise fail(p, f"goto {m.group(0)}: a delay slot")
        hit = [a for a in branches if a in span]
        if hit:
            raise fail(p, f"overlaps the branch patch at {hit[0]:08X}")
        by_start[p.start] = p
    return by_start, branches


def emit_patch(p: Patch, cl: Cluster) -> list[str]:
    out = [f"  /* native patch ({p.func}): {p.why} */"]
    for a in range(p.start, p.end, 4):
        out.append(f"  /* {a:08X} {cl.insns[a].word:08X} (replaced) */")
    out += [f"  {line}" if line else "" for line in p.c.strip("\n").splitlines()]
    return out


# ---- emit -------------------------------------------------------------------


@dataclass
class Ctx:
    sigs: dict[str, tuple[list[str], list[str]]]
    names: dict[int, str]  # every function address -> name
    cluster_of: dict[int, Cluster]  # hand-asm function entry -> its cluster
    externs: set[str] = field(default_factory=set)
    warnings: list[str] = field(default_factory=list)


def lab(a: int) -> str:
    return f"L_{a:08X}"


def c_call(ctx: Ctx, name: str, cl: Cluster, a: int) -> list[str]:
    """A jal to a game C function: call through its prototype's signature."""
    if name not in ctx.sigs:
        raise SystemExit(f"handasm: {cl.name}: no prototype for callee {name} ({a:08X})")
    params, results = ctx.sigs[name]
    if any(p != "i32" for p in params + results) or len(results) > 1:
        raise SystemExit(f"handasm: {name}: non-i32 signature {params} -> {results}")
    args = [REG[4 + k] if k < 4 else f"HA_LW(sp + {16 + 4 * (k - 4)}u)" for k in range(len(params))]
    ctx.externs.add(name)
    call = f"{name}({', '.join(args)})"
    out = ["{ uint32_t ha_save = ha_mips_sp; ha_mips_sp = sp;"]
    out.append(f"  {'v0 = (uint32_t)' if results else ''}{call};")
    out.append("  ha_mips_sp = ha_save; }")
    return out


def goto(cl: Cluster, t: int) -> str:
    # Entering a delay slot directly runs it without the branch (bc = 0).
    prev = cl.insns.get(t - 4)
    return f"{{ bc = 0; goto {lab(t)}; }}" if prev is not None and prev.kind else f"goto {lab(t)};"


def emit_transfer(ctx: Ctx, cl: Cluster, ins: Insn) -> list[str]:
    """The jump part of a control instruction (after its delay slot ran)."""
    t = ins.target
    if ins.kind in ("branch", "j"):
        if cl.owns(t):
            return [goto(cl, t)]
        if t in ctx.cluster_of:  # tail jump into another cluster
            other = ctx.cluster_of[t]
            return [
                "HA_SPILL();",
                f"ha_{other.name}(R, {t:#010x}u);",
                "HA_RELOAD();",
                "tgt = ra; goto dispatch;",
            ]
        raise SystemExit(f"handasm: {cl.name}: jump to {t:08X} outside hand-asm")
    if ins.kind in ("jal", "branch_link"):
        if cl.owns(t):
            return [goto(cl, t)]
        if t in ctx.cluster_of:
            other = ctx.cluster_of[t]
            return ["HA_SPILL();", f"ha_{other.name}(R, {t:#010x}u);", "HA_RELOAD();"]
        name = ctx.names.get(t)
        if not name:
            raise SystemExit(f"handasm: {cl.name}: jal to unknown {t:08X}")
        return c_call(ctx, name, cl, ins.addr)
    if ins.kind == "jr":
        return ["goto dispatch;"]
    raise SystemExit(f"handasm: {cl.name}: unsupported transfer {ins.kind} at {ins.addr:08X}")


def emit_cluster(ctx: Ctx, cl: Cluster) -> str:  # noqa: C901
    find_labels(cl)
    patches, branches = check_patches(cl)
    addrs = sorted(cl.insns)
    body: list[str] = []
    k = 0
    prev_falls = False
    prev_addr = None
    while k < len(addrs):
        a = addrs[k]
        ins = cl.insns[a]
        if prev_addr is not None and a != prev_addr + 4 and prev_falls:
            # Fell off the end of a function into a non-adjacent one.
            body.append(f"  tgt = {prev_addr + 4:#010x}u; goto dispatch;")
        if a in cl.labels:
            body.append(f"{lab(a)}:")
        if a in patches:
            p = patches[a]
            body += emit_patch(p, cl)
            prev_falls = True
            prev_addr = p.end - 4
            k += (p.end - p.start) // 4
            continue
        body.append(f"  /* {a:08X} {ins.word:08X} */")
        if ins.kind:
            # Control transfer + delay slot.
            d = cl.insns.get(a + 4)
            if d is None:
                raise SystemExit(f"handasm: {cl.name}: no delay slot after {a:08X}")
            if d.kind:
                raise SystemExit(f"handasm: {cl.name}: branch in delay slot at {a + 4:08X}")
            if d.load > 0:
                # The first instruction after the transfer sees the old value.
                nexts = [cl.insns.get(a + 8)]
                if ins.kind in ("branch", "j", "branch_link") or (
                    ins.kind == "jal" and cl.owns(ins.target)
                ):
                    nexts.append(cl.insns.get(ins.target))
                if ins.kind == "jal" and not cl.owns(ins.target):
                    nexts = []  # a C call runs first
                if any(n is not None and d.load in n.reads for n in nexts):
                    raise SystemExit(
                        f"handasm: {cl.name}: load-delay hazard after delay slot {a + 4:08X}"
                    )
            pre = []
            if a in branches:
                bp = branches[a]
                body.append(f"  /* native patch ({bp.func}): {bp.why} */")
                pre.append(f"bc = ({bp.cond});")
            elif ins.kind == "branch" or ins.kind == "branch_link":
                pre.append(f"bc = ({ins.cond});")
            elif ins.kind == "jr":
                pre.append(f"tgt = {R(ins.reg)};")
            if ins.kind in ("jal", "branch_link"):
                pre.append(f"ra = {a + 8:#010x}u;")
            if ins.kind == "jalr":
                raise SystemExit(f"handasm: {cl.name}: jalr at {a:08X}")
            delay_labeled = (a + 4) in cl.labels
            conditional = ins.kind in ("branch", "branch_link")
            if delay_labeled and not conditional:
                pre.append("bc = 1;")
            for p in pre:
                body.append(f"  {p}")
            if delay_labeled:
                body.append(f"{lab(a + 4)}:")
            body.append(f"  /* {a + 4:08X} {d.word:08X} (delay) */")
            if d.c:
                body.append(f"  {d.c}")
            xfer = emit_transfer(ctx, cl, ins)
            if conditional or delay_labeled:
                body.append("  if (bc) {")
                body += [f"    {x}" for x in xfer]
                body.append("  }")
            else:
                body += [f"  {x}" for x in xfer]
            # Unconditional transfers (j, jr) never fall through unless the
            # delay slot was entered directly (bc = 0).
            prev_falls = conditional or delay_labeled or ins.kind in ("jal", "branch_link")
            prev_addr = a + 4
            k += 2
            continue
        # Load delay: the next instruction sees the old register value.
        nxt = cl.insns.get(a + 4)
        pair = nxt is not None and ins.word >> 26 in (34, 38) and nxt.word >> 26 in (34, 38)
        if (
            ins.load > 0
            and not pair  # lwl/lwr forward to each other
            and nxt is not None
            and ins.load in nxt.reads
        ):
            if (a + 4) in cl.labels:
                raise SystemExit(f"handasm: {cl.name}: load-delay hazard into a label at {a:08X}")
            if nxt.kind:
                raise SystemExit(f"handasm: {cl.name}: load-delay hazard into a branch at {a:08X}")
            expr = ins.c.split(" = ", 1)[1].rstrip(";")
            body.append(f"  {{ uint32_t ld = {expr};")
            body.append(f"    /* {a + 4:08X} {nxt.word:08X} (load delay: old {REG[ins.load]}) */")
            if nxt.c:
                body.append(f"    {nxt.c}")
            if ins.load not in nxt.writes:
                body.append(f"    {REG[ins.load]} = ld; }}")
            else:
                body.append("  }")
            ctx.warnings.append(f"{cl.name}: load-delay hazard at {a:08X} (handled)")
            prev_falls = True
            prev_addr = a + 4
            k += 2
            continue
        if ins.c:
            body.append(f"  {ins.c}")
        prev_falls = True
        prev_addr = a
        k += 1
    if prev_falls and prev_addr is not None:
        body.append(f"  tgt = {prev_addr + 4:#010x}u; goto dispatch;")

    out = [
        f"/* cluster {cl.name}: " + ", ".join(f"{f.name} ({f.addr:08X})" for f in cl.funcs) + " */"
    ]
    out.append(f"void ha_{cl.name}(HaRegs *R, uint32_t entry) {{")
    out.append("  HA_LOCALS();")
    out.append("  uint32_t tgt = entry, entry_ra;")
    out.append("  int bc = 0;")
    out.append("  HA_RELOAD();")
    out.append("  entry_ra = ra;")
    out.append("  (void)bc;")
    out.append("  goto dispatch;")
    out += body
    out.append("dispatch:")
    out.append("  switch (tgt) {")
    for a in sorted(cl.dispatch):
        out.append(f"  case {a:#010x}u: {'bc = 0; ' if a in cl.labels else ''}goto {lab(a)};")
    out.append("  }")
    out.append("  if (tgt != entry_ra) {")
    out.append(f'    ha_bad_jump("{cl.name}", tgt, entry_ra);')
    out.append("  }")
    out.append("  HA_SPILL();")
    out.append("}")
    return "\n".join(out)


def emit_wrapper(ctx: Ctx, f: Func, cl: Cluster) -> str | None:
    sig = ctx.sigs.get(f.name)
    if sig is None:
        return None
    params, results = sig
    if any(p != "i32" for p in params + results) or len(results) > 1:
        raise SystemExit(f"handasm: {f.name}: non-i32 signature {params} -> {results}")
    ps = ", ".join(f"uint32_t p{k}" for k in range(len(params))) or "void"
    ret = "uint32_t" if results else "void"
    lines = [f"{ret} {f.name}({ps}) {{", "  HaRegs R;", "  ha_enter(&R);"]
    if len(params) > 4:
        # Stack arguments at the callee's sp+16.., as a PS1 caller passes them.
        frame = (16 + 4 * len(params) + 7) & ~7
        lines.append(f"  R.r[29] = ha_mips_sp - {frame:#x}u;")
    for k in range(len(params)):
        if k < 4:
            lines.append(f"  R.r[{4 + k}] = p{k};")
        else:
            lines.append(f"  HA_SW(R.r[29] + {16 + 4 * (k - 4)}u, p{k});")
    lines.append(f"  ha_{cl.name}(&R, {f.addr:#010x}u);")
    lines.append("  ha_leave(&R);")
    if results:
        lines.append("  return R.r[2];")
    lines.append("}")
    return "\n".join(lines)


HEADER = """\
/* GENERATED by `open-spyro native handasm` from asm/text.s — do not edit.
 * Literal translation of hand-written asm; see handasm.h. */
#include "handasm.h"
"""


def clang_format(files: list[Path], style: Path) -> None:
    """Formats FILES in place with STYLE, the repo's .clang-format (`make
    lint-c` checks the generated files too), using the pinned clang-format
    from this project's venv. The style is passed rather than found next to
    the files, so output written outside the repo formats the same."""
    exe = Path(sys.executable).with_name("clang-format")
    tool = str(exe) if exe.exists() else shutil.which("clang-format")
    if tool is None:
        raise SystemExit("handasm: clang-format not found (uv sync --group dev)")
    subprocess.run([tool, f"--style=file:{style}", "-i", *map(str, files)], check=True)


def assign_patches(clusters: list[Cluster], patches: list[Patch | BranchPatch]) -> None:
    """Hands each patch to the cluster holding its start."""
    for p in patches:
        at = p.addr if isinstance(p, BranchPatch) else p.start
        cl = next((c for c in clusters if c.owns(at)), None)
        if cl is None:
            raise SystemExit(f"handasm: patch {p.func} {at:08X}: not in any hand-asm cluster")
        cl.patches.append(p)


def run(out_dir: Path | None = None) -> None:
    root = repo_root()
    out = out_dir or root / OUT_DIR
    all_funcs = read_text_functions(root / "asm/text.s")
    # progress.json needs the rebuilt EXE; without it (CI's native job), the
    # list the committed translation was made from.
    progress = root / "progress.json"
    names = (
        handwritten_names(progress)
        if progress.exists()
        else (root / OUT_DIR / FUNCTIONS_TXT).read_text().split()
    )
    hw = [all_funcs[n] for n in names]
    clusters = build_clusters(hw, all_funcs)
    cluster_of = {f.addr: cl for cl in clusters for f in cl.funcs}
    assign_patches(clusters, PATCHES)
    hw_names = set(names)
    pulled = [f.name for cl in clusters for f in cl.funcs if f.name not in hw_names]
    if pulled:
        print(
            "handasm: translated with the hand-asm (reached by jump/fallthrough): "
            + ", ".join(pulled)
        )
    addr_names = {f.addr: f.name for f in all_funcs.values()}

    # Callees (jal targets outside hand-asm) and every hand-asm function C may call.
    callees = set()
    for cl in clusters:
        for ins in cl.insns.values():
            if (
                ins.kind in ("jal", "branch_link")
                and ins.target not in cluster_of
                and not cl.owns(ins.target)
            ):
                if ins.target in addr_names:
                    callees.add(addr_names[ins.target])
    probe = sorted(callees | {f.name for f in hw if not f.name.endswith("_Tail")})
    # funcs.h only declares what it declares; probe each name on its own so
    # one missing prototype does not fail the rest.
    declared = _declared(probe)
    sigs = c_signatures(sorted(declared))
    ctx = Ctx(sigs, addr_names, cluster_of)

    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob("ha_*.c"):
        old.unlink()
    index = [
        "/* GENERATED by `open-spyro native handasm` — do not edit. */",
        "#ifndef HANDASM_INDEX_H",
        "#define HANDASM_INDEX_H",
        '#include "handasm.h"',
    ]
    for cl in clusters:
        index.append(f"void ha_{cl.name}(HaRegs *R, uint32_t entry);")
    total_wrappers = 0
    for cl in clusters:
        ctx.externs = set()
        code = emit_cluster(ctx, cl)
        wrappers = [w for f in cl.funcs if f.name in hw_names and (w := emit_wrapper(ctx, f, cl))]
        total_wrappers += len(wrappers)
        ext = []
        for n in sorted(ctx.externs):
            params, results = ctx.sigs[n]
            ps = ", ".join("uint32_t" for _ in params) or "void"
            ext.append(f"extern {'uint32_t' if results else 'void'} {n}({ps});")
        text = HEADER + '#include "handasm_index.h"\n'
        for inc in sorted({i for p in cl.patches for i in p.includes}):
            text += f'#include "{inc}"\n'
        text += "\n"
        if ext:
            text += "\n".join(ext) + "\n\n"
        text += code + "\n"
        if wrappers:
            text += "\n" + "\n\n".join(wrappers) + "\n"
        (out / f"ha_{cl.name}.c").write_text(text)
    index.append("#endif")
    (out / "handasm_index.h").write_text("\n".join(index) + "\n")
    (out / FUNCTIONS_TXT).write_text("".join(f"{n}\n" for n in names))
    clang_format(sorted(out.glob("*.[ch]")), root / ".clang-format")
    hazards = [w for w in ctx.warnings if "handled" in w]
    other = [w for w in ctx.warnings if "handled" not in w]
    for w in other:
        print(f"handasm: warning: {w}")
    n_insn = sum(len(cl.insns) for cl in clusters)
    print(
        f"handasm: {len(hw)} functions ({n_insn} instructions) in {len(clusters)} clusters, "
        f"{total_wrappers} C wrappers, {len(hazards)} load-delay hazards, "
        f"{len(PATCHES)} native patches -> {out}"
    )


def _declared(names: list[str]) -> set[str]:
    """The names funcs.h declares (a prototype probe fails on the others)."""
    text = (repo_root() / "include/funcs.h").read_text()
    return {n for n in names if re.search(rf"[\s*]{re.escape(n)}\s*\(", text)}
