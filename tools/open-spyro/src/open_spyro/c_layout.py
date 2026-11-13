"""Which C file defines which function.

A source file may define any number of functions. Each function still links into
its own fixed-VMA slot: ``config/compile.sh`` gives every function its own
``.text.<fn>`` / ``.rodata.<fn>`` sections (``asm_sections``), and gen-slots-ld
places ``build/<seg>/c/<path>.o(.text.<fn>)`` at the function's address. So the
grouping of functions into files, and of files into directories, never moves a byte.

This module is the one place that answers "is <fn> C, and where": a scan of each
file's top-level function definitions. ``HAVE_C_<fn>`` flags, the slot fragment,
progress and the per-function tools all read it. The build cross-checks the scan
against the ``.text.*`` sections each object actually contains
(``check_objects``), so a definition the scanner misreads fails the build instead
of silently linking asm.

Rules the scan relies on (all enforced by the build):
  * function names are unique per segment (objects mirror the source tree, so
    file names need not be);
  * ``inline`` helpers are not slot functions: gcc must inline them, never emit
    them (an emitted one shows up in ``check_objects``);
  * parked ``.c.wip`` attempts stay one function per file, named after it.
"""

from __future__ import annotations

import os
import re
import subprocess
from pathlib import Path

from open_spyro.paths import c_sources, segment_src_root

# Slots that splat split off as their own symbol but that the original emitted as
# the tail of the PRECEDING function (fragment -> owner). The owner's object fills
# the fragment's bytes too, so the fragment counts as C whenever its owner is, and
# gen-slots-ld drops the fragment's anchor (the owner's object runs through it).
ABSORBED_BY = {
    "main": {
        # main()'s dead epilogue (0x8001228c, 28 bytes): gcc emits it after the
        # infinite frame loop and splat split it off as its own symbol.
        "func_8001228C": "main",
    },
}

# Comments, string / char literals, and preprocessor lines (with continuations).
NOISE = re.compile(
    r"/\*.*?\*/|//[^\n]*|\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'|^[ \t]*#(?:\\\n|[^\n])*",
    re.S | re.M,
)
_IDENT_BEFORE = re.compile(r"([A-Za-z_]\w*)\s*$")
_NOT_A_NAME = {"if", "while", "for", "switch", "return", "sizeof", "__attribute__", "asm"}


def _definition_name(decl: str) -> str | None:
    """The function a top-level ``decl {`` defines, or None (struct, initializer, ...)."""
    decl = decl.strip()
    if not decl.endswith(")") or re.search(r"\binline\b|\b__inline__\b", decl):
        return None
    depth = 0
    for i in range(len(decl) - 1, -1, -1):
        if decl[i] == ")":
            depth += 1
        elif decl[i] == "(":
            depth -= 1
            if depth == 0:
                head = decl[:i]
                if "=" in head:
                    return None
                m = _IDENT_BEFORE.search(head)
                if m and m.group(1) not in _NOT_A_NAME:
                    return m.group(1)
                return None
    return None


def defined_functions(text: str) -> list[str]:
    """Names of the (non-inline) functions defined at file scope, in source order."""
    src = NOISE.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), text)
    names: list[str] = []
    depth = 0
    start = 0  # where the current top-level declaration began
    for i, ch in enumerate(src):
        if ch == "{":
            if depth == 0:
                name = _definition_name(src[start:i])
                if name:
                    names.append(name)
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                start = i + 1
        elif ch == ";" and depth == 0:
            start = i + 1
    return names


def function_files(repo: Path, segment: str, suffix: str = ".c") -> dict[str, Path]:
    """function name -> defining file, for every ``*<suffix>`` in ``segment``.

    Refuses a function defined twice. For ``.c`` the absorbed fragments of a
    present owner map to the owner's file.
    """
    out: dict[str, Path] = {}
    for p in c_sources(segment_src_root(repo, segment), suffix):
        stem = p.name.removesuffix(suffix)
        # A parked attempt may be an m2c draft that does not parse; it is named
        # after its function by convention, so fall back to the stem.
        names = defined_functions(p.read_text()) if suffix == ".c" else [stem]
        for name in names:
            if name in out:
                raise SystemExit(f"open-spyro: {name} is defined in both {out[name]} and {p}")
            out[name] = p
    if suffix == ".c":
        for frag, owner in ABSORBED_BY.get(segment, {}).items():
            if owner in out:
                out[frag] = out[owner]
    return out


def find_c_source(repo: Path, segment: str, name: str, suffix: str = ".c") -> Path | None:
    """The file defining function ``name`` in ``segment``, wherever it is grouped."""
    return function_files(repo, segment, suffix).get(name)


def object_name(repo: Path, segment: str, src: Path) -> str:
    """The object a source compiles to, relative to ``build/<seg>/c/`` and without ``.o``
    (the source's path under its segment root: ``actor/movement``)."""
    return str(src.relative_to(segment_src_root(repo, segment)).with_suffix(""))


def check_objects(repo: Path, segment: str, c_dir: str) -> None:
    """Fail unless each object's ``.text.*`` sections are exactly its scanned functions."""
    by_file: dict[Path, set[str]] = {}
    absorbed = ABSORBED_BY.get(segment, {})
    for name, src in function_files(repo, segment).items():
        if name not in absorbed:
            by_file.setdefault(src, set()).add(name)
    srcs = c_sources(segment_src_root(repo, segment))
    objs = [repo / c_dir / f"{object_name(repo, segment, p)}.o" for p in srcs]
    if not objs:
        return
    dump = subprocess.run(
        [os.environ.get("OBJDUMP", "mips-linux-gnu-objdump"), "-h", *map(str, objs)],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    found: dict[str, set[str]] = {}
    cur = ""
    for line in dump.splitlines():
        if ":     file format" in line:
            obj = Path(line.split(":", 1)[0]).resolve()
            cur = str(obj.relative_to((repo / c_dir).resolve()).with_suffix(""))
            found[cur] = set()
            continue
        parts = line.split()
        if len(parts) > 2 and parts[1].startswith(".text.") and int(parts[2], 16):
            found[cur].add(parts[1].removeprefix(".text."))
    bad = []
    for src in srcs:
        want, got = by_file.get(src, set()), found.get(object_name(repo, segment, src), set())
        if want != got:
            bad.append(
                f"    {src.relative_to(repo)}: scanned {sorted(want)}, object has {sorted(got)}"
            )
    if bad:
        raise SystemExit(
            "open-spyro: C files and their objects disagree on which functions they "
            "define:\n" + "\n".join(bad)
        )


def havec_defsyms(repo: Path, segment: str) -> str:
    """``--defsym HAVE_C_<fn>=1`` for every C function of ``segment`` (assembler flags)."""
    return " ".join(f"--defsym HAVE_C_{n}=1" for n in sorted(function_files(repo, segment)))
