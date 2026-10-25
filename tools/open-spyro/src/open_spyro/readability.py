"""Readability-debt report for the matched C (phase 2: make it readable, keep it matching).

Every matched function already rebuilds byte-identically; this measures how far
each source file is from reading like real code, so readability sessions can
pick targets the way match sessions use ``TRIAGE.md``. Pure stdlib, disc-free,
deterministic.

Signals (per ``src/**/*.c`` file; weights in ``WEIGHTS``):

  alias       autolabel ref (``func_8001…``/``D_8007…``) whose address main
              already names — fixed mechanically by ``open-spyro canonicalize``
  anon_func   placeholder function name (``func_<ADDR>``, ``func_<seg>_<ADDR>``, ``FUN_``),
              including the file's own function
  anon_data   placeholder data name (``D_<ADDR>``, ``DAT_``, ``unk_…``) incl. views,
              and struct fields still named by offset (``st->u2C``)
  m2c_local   decompiler / permuter scaffolding locals (m2c ``temp_*``, ``var_*``,
              ``spXX``, ``argN``; Ghidra ``iVar1``, ``param_1``, ``local_10``;
              permuter ``new_var``; frame-offset names ``f168``)
  raw_offset  pointer-cast field access (``*(int *)(p + 0x44)``) or any
              byte- or int-cast base plus a field-sized constant (``(char *)p + 0x44``),
              however parenthesized, or a pointer cast of one (``(int *)(p + 0xC)``) —
              a struct waiting to be recovered.  Sites
              inside a ``#define`` (a named accessor) are not counted.
  extern      file-local ``extern`` declarations (header-able duplicates)

Matching scaffolding (``asm`` barriers, register pins, ``do {} while (0)``) is
counted as ``asm`` for information only — it is load-bearing and not debt.

Emits ``READABILITY.md`` (tracked) and ``build/readability.json``.
"""

from __future__ import annotations

import json
import re
from collections import Counter
from pathlib import Path

from open_spyro import rename
from open_spyro.paths import repo_root

WEIGHTS = {
    "alias": 1,
    "anon_func": 3,
    "anon_data": 2,
    "m2c_local": 1,
    "raw_offset": 2,
    "extern": 0.25,
}

_ANON_FUNC = re.compile(
    r"(?<![A-Za-z0-9_])(?:func_(?:[a-z0-9_]+_)?[0-9A-F]{8}|FUN_[0-9a-fA-F]{8})(?![A-Za-z0-9])"
)
_ANON_DATA = re.compile(
    r"(?<![A-Za-z0-9_])(?:D_[0-9A-F]{8}|DAT_[0-9a-fA-F]{8}|unk_?[0-9A-Fa-fx]+)"
    r"(?:_[A-Za-z0-9]+)?\b"
)
_M2C = re.compile(
    r"\b(?:temp_[a-z0-9_]+|var_[a-z0-9_]+|sp[0-9A-F]{2,3}|arg[0-9]"
    r"|new_var[0-9]*|[a-z]{1,3}Var[0-9]+|param_[0-9]+|local_[0-9a-f]+|f[0-9]{3})\b"
)
_RAW = re.compile(
    r"\*\s*\(+\s*(?:volatile\s+)?(?:unsigned\s+|signed\s+)?"
    r"(?:int|short|char|long|s8|u8|s16|u16|s32|u32)\s*\*\s*\)\s*\(+\s*"
    r"(?:\(\s*(?:unsigned\s+|signed\s+)?(?:char|u8|s8)\s*\*\s*\)\s*)?[A-Za-z_]\w*\s*\)?\s*\+"
)
# A byte- or int-cast base plus a constant field offset (< 0x400), however
# parenthesized: ``*((unsigned char *)(((char *)p) + 0x46))``,
# ``(DISPENV *)((char *)env + 0x5c)``, ``(int)rec + 0x13``.
_BYTE_OFF = re.compile(
    r"\(\s*(?:(?:unsigned\s+|signed\s+)?(?:char|u8|s8)\s*\*|int|u32|unsigned\s+int)\s*\)\s*"
    r"(?:\(\s*[A-Za-z_]\w*(?:->\w+)?\s*\)|[A-Za-z_]\w*(?:->\w+)?\s*\)?)"
    r"\s*(\+)\s*(?:0x[0-9a-fA-F]{1,3}|\d{1,3})\b"
)
# A pointer cast of ``base + constant`` (``(int *)(actor + 0xC)``,
# ``(DISPENV *)(env + 0x5c)``, ``(int **)(rec + 0x18)``): a field's address.
_PTR_OFF = re.compile(
    r"\(\s*(?:volatile\s+)?(?:unsigned\s+|signed\s+)?(?:int|short|char|long|void|[us](?:8|16|32)"
    r"|[A-Z]\w*)\s*\*+\s*\)\s*\(\s*[A-Za-z_]\w*\s*(\+)\s*(?:0x[0-9a-fA-F]{1,3}|\d{1,3})\s*\)"
)
_DEFINE = re.compile(r"^[ \t]*#[ \t]*define\b.*?(?<!\\)$", re.M | re.S)
# A struct field still named by its offset (``st->u2C``, ``rec.u88``).
_ANON_FIELD = re.compile(r"(?:->|\.)(u[0-9A-F]{2,3})\b")
_EXTERN = re.compile(r"^extern\b", re.M)
_ASM = re.compile(r"\b(?:__)?asm(?:__)?\s*(?:volatile\s*|__volatile__\s*)?\(")
_COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)


def _strip_comments(text: str) -> str:
    return _COMMENT.sub(" ", text)


def _raw_offsets(code: str) -> int:
    """Distinct raw-offset sites, keyed by the position of their ``+``."""
    body = _DEFINE.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), code)
    sites = {m.end() - 1 for m in _RAW.finditer(body)}
    sites |= {m.start(1) for m in _BYTE_OFF.finditer(body)}
    sites |= {m.start(1) for m in _PTR_OFF.finditer(body)}
    return len(sites)


def scan(root: Path) -> list[dict]:
    rels = sorted(
        p.relative_to(root) for d in ("src/c", "src/overlays") for p in (root / d).rglob("*.c")
    )
    aliases = rename.alias_candidates(root, rels)
    rows = []
    for rel in rels:
        code = _strip_comments((root / rel).read_text())
        alias_ids = set(aliases.get(rel, {}))
        anon_f = {m for m in _ANON_FUNC.findall(code)} - alias_ids
        anon_d = {m for m in _ANON_DATA.findall(code)} - alias_ids
        anon_d |= {"." + m for m in _ANON_FIELD.findall(code)}
        sig = {
            "alias": len(alias_ids),
            "anon_func": len(anon_f),
            "anon_data": len(anon_d),
            "m2c_local": len(set(_M2C.findall(code))),
            "raw_offset": _raw_offsets(code),
            "extern": len(_EXTERN.findall(code)),
        }
        seg = rel.parts[2] if rel.parts[1] == "overlays" else "main"
        rows.append(
            {
                "file": str(rel),
                "function": rel.stem,
                "segment": seg,
                "lines": code.count("\n"),
                "asm": len(_ASM.findall(code)),
                **sig,
                "score": round(sum(WEIGHTS[k] * v for k, v in sig.items()), 2),
            }
        )
    rows.sort(key=lambda r: (-r["score"], r["file"]))
    return rows


def _md(rows: list[dict], top: int) -> str:
    tot = Counter()
    for r in rows:
        tot.update({k: r[k] for k in (*WEIGHTS, "asm", "score")})
    clean = sum(1 for r in rows if r["score"] == 0)
    seg_rows: dict[str, list[float]] = {}
    for r in rows:
        seg_rows.setdefault(r["segment"], []).append(r["score"])
    out = [
        "# Readability — phase-2 cleanup targets",
        "",
        "Generated by `make readability` (`open-spyro readability`). Every file here "
        "already matches byte-for-byte; the score measures naming/typing debt, not "
        "correctness. Lower is better. `make check` gates every cleanup.",
        "",
        "## Totals",
        "",
        f"- files: {len(rows)} ({clean} at score 0)",
        f"- debt score: {tot['score']:.0f}",
        "",
        "| Signal | Count | Fix |",
        "|---|--:|---|",
        f"| alias refs | {tot['alias']} | `open-spyro canonicalize` (mechanical) |",
        f"| placeholder functions | {tot['anon_func']} | `open-spyro rename` |",
        f"| placeholder data | {tot['anon_data']} | `open-spyro rename` |",
        f"| m2c locals | {tot['m2c_local']} | edit in place |",
        f"| raw offset casts | {tot['raw_offset']} | recover a struct in `include/` |",
        f"| file-local externs | {tot['extern']} | move to `include/` headers |",
        f"| asm scaffolding (info) | {tot['asm']} | load-bearing — keep |",
        "",
        "## By segment",
        "",
        "| Segment | Files | Score |",
        "|---|--:|--:|",
    ]
    for seg, scores in sorted(seg_rows.items(), key=lambda kv: -sum(kv[1])):
        out.append(f"| {seg} | {len(scores)} | {sum(scores):.0f} |")
    out += [
        "",
        f"## Top {top} files",
        "",
        "| File | Score | alias | anon fn | anon data | m2c | offsets | externs | asm |",
        "|---|--:|--:|--:|--:|--:|--:|--:|--:|",
    ]
    for r in rows[:top]:
        out.append(
            f"| `{r['file']}` | {r['score']:g} | {r['alias']} | {r['anon_func']} | "
            f"{r['anon_data']} | {r['m2c_local']} | {r['raw_offset']} | {r['extern']} | "
            f"{r['asm']} |"
        )
    return "\n".join(out) + "\n"


def run(top: int) -> None:
    root = repo_root()
    rows = scan(root)
    (root / "READABILITY.md").write_text(_md(rows, top))
    build = root / "build"
    build.mkdir(exist_ok=True)
    (build / "readability.json").write_text(json.dumps(rows, indent=1) + "\n")
    score = sum(r["score"] for r in rows)
    print(f"readability: {len(rows)} files, debt score {score:.0f} -> READABILITY.md")
