"""Merge several C files of one segment into a single translation unit.

The functions are written in address order (the order the original linker laid
them out). Each input file becomes one block, copied verbatim except for its
``#include`` lines, which are hoisted (deduplicated, sorted as clang-format
would) to the top of the output. A block's own ``#define``s are ``#undef``ed
after it, so every macro stays scoped to the function(s) it was written for —
several files reuse a macro name with a different body. (Not a macro a header
also defines: a block's ``#ifndef``-guarded copy of it must not take the
header's away from later blocks.) A ``static inline`` helper an earlier block
already defined verbatim is dropped from later blocks.

What a merge can still change, and what ``make build && make verify`` will catch:
  * a declaration in one block that conflicts with a header another block
    included (local redeclarations often pick a type to steer codegen);
  * a file-scope ``register`` variable, which affects every later function and
    must come before the first function definition (keep such files separate);
  * an ``SDATA()`` small-data declaration, which applies to the whole file
    (refused here when another block accesses the symbol);
  * a prototype newly visible to a function that was matched without it.

The inputs are deleted once the output is written.
"""

from __future__ import annotations

import json
import re
from pathlib import Path

from open_spyro.c_layout import NOISE, defined_functions
from open_spyro.paths import repo_root

_INCLUDE = re.compile(r"^\s*#\s*include\b")
_DEFINE = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)", re.M)
_SDATA = re.compile(r"^\s*SDATA\(\s*([A-Za-z_]\w*)", re.M)
_WORD = re.compile(r"[A-Za-z_]\w*")


def _addresses(repo: Path) -> dict[str, int]:
    layouts = [
        repo / "config/text_layout.json",
        *(repo / "config/overlays").glob("*.text_layout.json"),
    ]
    out: dict[str, int] = {}
    for p in layouts:
        for f in json.loads(p.read_text())["functions"]:
            if f["vram"] is not None:
                out.setdefault(f["name"], f["vram"])
    return out


def _inline_definitions(text: str) -> list[tuple[int, int]]:
    """(start, end) of each top-level ``static inline`` function definition in text,
    widened to the comment directly above it."""
    blank = NOISE.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), text)
    spans = []
    for m in re.finditer(r"^static\s+inline\b[^;{]*\{", blank, re.M):
        depth, i = 0, m.end() - 1
        for i in range(m.end() - 1, len(blank)):
            depth += {"{": 1, "}": -1}.get(blank[i], 0)
            if depth == 0:
                break
        start = m.start()
        above = re.search(r"/\*(?:[^*]|\*(?!/))*\*/\n$", text[:start])
        spans.append((above.start() if above else start, i + 1))
    return spans


def _drop_repeated_inlines(block: str, seen: set[str]) -> str:
    """Remove the ``static inline`` helpers an earlier block already defined verbatim."""
    out, last = [], 0
    for start, end in _inline_definitions(block):
        body = block[start:end]
        key = re.sub(r"/\*.*?\*/|\s+", "", body, flags=re.S)
        if key in seen:
            out.append(block[last:start].rstrip("\n") + "\n")
            last = end
        else:
            seen.add(key)
    out.append(block[last:])
    return "".join(out)


def _check_sdata(srcs: list[Path]) -> None:
    """Refuse a merge that would make another block's accesses $gp-relative.

    ``SDATA(sym, n)`` (include/sdata.h) is per translation unit: from there on,
    every access to ``sym`` in the file assembles $gp-relative. A function written
    against absolute (lui/%lo) accesses of ``sym`` would silently change.
    """
    sdata = {p: set(_SDATA.findall(p.read_text())) for p in srcs}
    clashes = []
    for p in srcs:
        words = set(_WORD.findall(NOISE.sub(" ", p.read_text())))
        for q in srcs:
            if q != p:
                for sym in sorted((sdata[q] - sdata[p]) & words):
                    clashes.append(f"    {sym}: SDATA in {q}, accessed without it in {p}")
    if clashes:
        raise SystemExit(
            "merge-c: SDATA is per translation unit; merging would make these "
            "accesses $gp-relative:\n" + "\n".join(clashes)
        )


def _header_macros(repo: Path) -> set[str]:
    """Macros some project header defines: a block's guarded copy must not be #undef'ed."""
    headers = [*(repo / "include").glob("*.h"), *(repo / "src").rglob("*.h")]
    return {m for h in headers for m in _DEFINE.findall(h.read_text())}


def merge(out: Path, srcs: list[Path]) -> None:
    repo = repo_root()
    addr = _addresses(repo)
    shared = _header_macros(repo)

    def first_addr(p: Path) -> int:
        names = defined_functions(p.read_text())
        if not names:
            raise SystemExit(f"merge-c: {p} defines no function")
        missing = [n for n in names if n not in addr]
        if missing:
            raise SystemExit(f"merge-c: {p}: no layout address for {', '.join(missing)}")
        return min(addr[n] for n in names)

    srcs = sorted(dict.fromkeys(srcs), key=first_addr)
    if out.exists() and out.resolve() not in {s.resolve() for s in srcs}:
        raise SystemExit(f"merge-c: {out} already exists")
    _check_sdata(srcs)
    includes: list[str] = []
    blocks: list[str] = []
    inlines: set[str] = set()
    for p in srcs:
        text = p.read_text()
        body: list[str] = []
        for line in text.splitlines():
            if _INCLUDE.match(line):
                if line.strip() not in includes:
                    includes.append(line.strip())
            else:
                body.append(line)
        block = _drop_repeated_inlines("\n".join(body).strip("\n") + "\n", inlines)
        local = [d for d in dict.fromkeys(_DEFINE.findall(text)) if d not in shared]
        undefs = "".join(f"#undef {d}\n" for d in local)
        blocks.append(block + ("\n" + undefs if undefs else ""))
    merged = "\n".join(sorted(includes)) + "\n\n" + "\n".join(blocks)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(merged)
    for p in srcs:
        if p.resolve() != out.resolve():
            p.unlink()
    print(f"merge-c: {len(srcs)} files -> {out}")
