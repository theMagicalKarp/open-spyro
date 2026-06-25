"""Per-function sectioning of asm/text.s (C-flip infra).

splat emits asm/text.s as ONE ``.section .text`` blob: 737 functions back-to-back,
each preceded by a ``nonmatching <name>[, <size>]`` marker + a ``glabel``/``dlabel``.
A C match must replace exactly one function's bytes while every other function stays
pinned at its original VMA — impossible inside a single monolithic section
(removing/replacing one function shifts all that follow).

This rewrites text.s so each function lives in its own ``.section .text.<name>``
guarded by ``.ifndef HAVE_C_<name>``:

    .section .text.Foo, "ax"
    .ifndef HAVE_C_Foo
    nonmatching Foo, 0x40
    glabel Foo
        ... body ...
    .endif

The fixed-VMA linker fragment (gen-slots-ld) then anchors each section at its
address, and a C override (src/c/Foo.c) suppresses the asm via
``--defsym HAVE_C_Foo=1`` (driven by tools/build_main.sh) and fills the slot instead.

Also writes config/text_layout.json — the ordered (name, vram, size) inventory the
slot generator consumes. That inventory is captured from the COMPLETE splat output,
so it survives even after a function's asm body is suppressed.

Idempotent: strips any sectioning it previously added before re-wrapping, so it is
safe to re-run (and to run after every ``make split``).
"""

from __future__ import annotations

import json
import re
import sys

from open_spyro.paths import repo_root

_NONMATCHING = re.compile(r"^nonmatching\s+(\S+?)(?:,\s*(0x[0-9A-Fa-f]+|\d+))?\s*$")
_INSTR_VRAM = re.compile(r"/\*\s*[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})\s+[0-9A-Fa-f]{8}\s*\*/")
# Lines this tool itself injects — stripped on re-run so wrapping stays idempotent.
_INJECTED = re.compile(r'^\.section \.text\.\S+, "ax"$|^\.ifndef HAVE_C_\S+$|^\.endif$')


def _parse_blocks(lines: list[str]) -> tuple[list[str], list[list[str]]]:
    """Split the raw (de-sectioned) lines into (header_lines, [block_lines...]).

    A block starts at a ``nonmatching`` marker and runs to the next one (or EOF).
    Everything before the first marker is the file header.
    """
    starts = [i for i, ln in enumerate(lines) if _NONMATCHING.match(ln)]
    if not starts:
        return lines, []
    header = lines[: starts[0]]
    blocks = []
    for n, s in enumerate(starts):
        e = starts[n + 1] if n + 1 < len(starts) else len(lines)
        blocks.append(lines[s:e])
    return header, blocks


def _block_info(block: list[str]) -> tuple[str, int, int | None]:
    """(name, size, vram) for a block. size from the marker, else VMA span."""
    m = _NONMATCHING.match(block[0])
    assert m is not None
    name = m.group(1)
    explicit = int(m.group(2), 0) if m.group(2) else None
    vrams = [int(mm.group(1), 16) for ln in block if (mm := _INSTR_VRAM.search(ln))]
    vram = vrams[0] if vrams else None
    if explicit is not None:
        size = explicit
    elif vrams:
        size = vrams[-1] + 4 - vrams[0]
    else:
        size = 0
    return name, size, vram


def run() -> None:
    repo = repo_root()
    text_s = repo / "asm/text.s"
    layout_path = repo / "config/text_layout.json"

    raw = text_s.read_text().splitlines()
    # Idempotency: drop any sectioning/guards a previous run added, and the
    # original single `.section .text, "ax"`, so we wrap a clean blob each time.
    raw = [ln for ln in raw if not _INJECTED.match(ln) and ln.strip() != '.section .text, "ax"']

    header, blocks = _parse_blocks(raw)

    out = list(header)
    # Re-establish the section state the monolithic file used to set once up top.
    out.append('.section .text, "ax"')
    out.append("")

    layout = []
    for block in blocks:
        name, size, vram = _block_info(block)
        layout.append({"name": name, "vram": vram, "size": size})
        out.append(f'.section .text.{name}, "ax"')
        out.append(f".ifndef HAVE_C_{name}")
        # trim leading/trailing blank lines inside the block for tidy output
        body = block
        while body and body[-1].strip() == "":
            body = body[:-1]
        out.extend(body)
        out.append(".endif")
        out.append("")

    text_s.write_text("\n".join(out) + "\n")

    missing = [b["name"] for b in layout if b["vram"] is None]
    if missing:
        print(
            f"sectionize: WARNING {len(missing)} blocks have no VMA: {missing[:5]}",
            file=sys.stderr,
        )
    layout_path.write_text(json.dumps({"functions": layout}, indent=1) + "\n")
    print(
        f"sectionize: wrapped {len(blocks)} functions -> {text_s.relative_to(repo)}; "
        f"layout -> {layout_path.relative_to(repo)}"
    )
