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


def _sectionize_file(text_s, layout_path) -> int:
    """Wrap every function block of one splat text.s; write its layout inventory.

    Shared by the main EXE (asm/text.s) and split overlays (asm/overlays/<n>/text.s):
    both are splat output with the same ``nonmatching``/``glabel`` block format.
    Returns the number of functions wrapped.
    """
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
    return len(blocks)


def _patch_main_ld(repo) -> None:
    """Re-insert the slots INCLUDE into splat's regenerated main ld (idempotent).

    splat rewrites config/spyro.main.ld on every split, wiping the INCLUDE that
    places the per-function fixed-VMA slots (and the C overrides) — the main-EXE
    twin of _patch_overlay_ld. Without it every HAVE_C-suppressed function links
    undefined.
    """
    ld_path = repo / "config/spyro.main.ld"
    target = "build/main/asm/text.o(.text);"
    insert = (
        "        /* Per-function fixed-VMA slots (`open-spyro gen-slots-ld`). Replaces the old",
        "           monolithic text.o(.text) so individual functions can be flipped to C. */",
        "        INCLUDE config/spyro.main.slots.ld",
    )
    lines = ld_path.read_text().splitlines()
    if any(ln.strip() == insert[-1].strip() for ln in lines):
        return
    for i, ln in enumerate(lines):
        if ln.strip() == target:
            lines[i:i] = insert
            break
    else:
        raise SystemExit(f"sectionize: no '{target}' line in {ld_path}")
    ld_path.write_text("\n".join(lines) + "\n")


def run() -> None:
    repo = repo_root()
    text_s = repo / "asm/text.s"
    layout_path = repo / "config/text_layout.json"
    n = _sectionize_file(text_s, layout_path)
    _patch_main_ld(repo)
    print(
        f"sectionize: wrapped {n} functions -> {text_s.relative_to(repo)}; "
        f"layout -> {layout_path.relative_to(repo)}"
    )


# Offset comments in splat's data dump: `/* <off> <vram8> [<bytes8>] */`. The
# vram token is required so raw continuation lines (`/* 4241... */`) don't match.
_RODATA_OFF = re.compile(r"^\s*/\*\s*([0-9A-Fa-f]+)\s+[0-9A-Fa-f]{8}\s")
# Lines _sectionize_rodata injects — stripped on re-run for idempotency.
_INJECTED_RO = re.compile(r'^\.section \.rodata\.\S+, "a"$|^\.ifndef HAVE_C_\S+$|^\.endif$')


def _sectionize_rodata(rodata_s, text_s, text_end_off: int) -> list[dict]:
    """Per-piece sectioning of an overlay's rodata dump (jtbl / const-data flips).

    A C override whose function owns rodata (a switch jump table, a static const)
    emits those bytes in its object's ``.rodata``; the matching asm bytes must be
    suppressible. Wraps every ``nonmatching <sym>`` block of the splat rodata dump
    in its own ``.rodata.<sym>`` section, guarded by ``.ifndef HAVE_C_<owner>``
    where the owner is the (unique) function in text.s that references <sym>.
    Returns the piece inventory [(sym, offset, size, owner)...] for the slot
    generator. Idempotent, same as the text sectionizer.
    """
    raw = rodata_s.read_text().splitlines()
    raw = [ln for ln in raw if not _INJECTED_RO.match(ln) and ln.strip() != '.section .rodata, "a"']
    header, blocks = _parse_blocks(raw)

    text = text_s.read_text()
    # function name -> block body, from the already-sectionized text.s
    func_bodies: dict[str, str] = {}
    for m in re.finditer(r"\.ifndef HAVE_C_(\S+)\n(.*?)\n\.endif", text, re.S):
        func_bodies[m.group(1)] = m.group(2)

    out = list(header)
    out.append('.section .rodata, "a"')
    out.append("")

    pieces: list[dict] = []
    for block in blocks:
        m = _NONMATCHING.match(block[0])
        assert m is not None
        sym = m.group(1)
        offs = [int(mm.group(1), 16) for ln in block if (mm := _RODATA_OFF.match(ln))]
        owner = next((fn for fn, body in func_bodies.items() if f"({sym})" in body), None)
        pieces.append({"sym": sym, "offset": offs[0] if offs else None, "owner": owner})
        out.append(f'.section .rodata.{sym}, "a"')
        if owner:
            out.append(f".ifndef HAVE_C_{owner}")
        body = block
        # Drop trailing `.align` directives (splat emits `.align 3` after each
        # jump table): in the monolithic .rodata they are no-ops (pad words are
        # dumped explicitly), but inside a carved piece section they pad the
        # SECTION size past its fixed-VMA slot anchor — the anchor, not the
        # directive, owns placement/padding here.
        while body and (body[-1].strip() == "" or body[-1].strip().startswith(".align")):
            body = body[:-1]
        out.extend(body)
        if owner:
            out.append(".endif")
        out.append("")

    # piece sizes from consecutive offsets; the last piece runs to .text start
    for p, nxt in zip(pieces, pieces[1:] + [{"offset": text_end_off}], strict=True):
        p["size"] = (
            nxt["offset"] - p["offset"]
            if p["offset"] is not None and nxt["offset"] is not None
            else None
        )
    rodata_s.write_text("\n".join(out) + "\n")
    return pieces


def _patch_overlay_ld(repo, name: str) -> None:
    """Insert the slots INCLUDEs into splat's generated overlay ld (idempotent).

    splat regenerates config/overlays/<name>.ld on every split, wiping the
    includes; this re-inserts them just before the monolithic rodata/text object
    placements — the same INCLUDE-before-text pattern as config/spyro.main.ld.
    """
    ld_path = repo / "config/overlays" / f"{name}.ld"
    obj_base = f"build/overlays/{name}/asm/overlays/{name}"
    inserts = {
        f"{obj_base}/data/rodata.rodata.o(.rodata);": (
            "        /* Per-piece fixed-VMA rodata slots (`open-spyro gen-slots-ld"
            f" --overlay {name}`). */",
            f"        INCLUDE config/overlays/{name}.rodata_slots.ld",
        ),
        f"{obj_base}/text.o(.text);": (
            "        /* Per-function fixed-VMA text slots (`open-spyro gen-slots-ld"
            f" --overlay {name}`). */",
            f"        INCLUDE config/overlays/{name}.slots.ld",
        ),
    }
    lines = ld_path.read_text().splitlines()
    for target, insert in inserts.items():
        if any(ln.strip() == insert[1].strip() for ln in lines):
            continue
        for i, ln in enumerate(lines):
            if ln.strip() == target:
                lines[i:i] = insert
                break
        else:
            raise SystemExit(f"sectionize-overlays: no '{target}' line in {ld_path}")
    ld_path.write_text("\n".join(lines) + "\n")


def run_overlays() -> None:
    """Sectionize every function/data-split overlay (those with a symbols seed).

    Overlays without config/overlays/<name>.symbols.txt are coarse asm blobs —
    left monolithic (and `unsplit` in the progress metric).
    """
    repo = repo_root()
    overlays = json.loads((repo / "config/overlays.json").read_text())["overlays"]
    done = []
    for r in overlays:
        name = r["name"]
        if not (repo / "config/overlays" / f"{name}.symbols.txt").exists():
            continue
        text_s = repo / "asm/overlays" / name / "text.s"
        rodata_s = repo / "asm/overlays" / name / "data" / "rodata.rodata.s"
        layout_path = repo / "config/overlays" / f"{name}.text_layout.json"
        n = _sectionize_file(text_s, layout_path)
        pieces = _sectionize_rodata(rodata_s, text_s, r["text_off"])
        layout = json.loads(layout_path.read_text())
        layout["rodata"] = pieces
        layout_path.write_text(json.dumps(layout, indent=1) + "\n")
        _patch_overlay_ld(repo, name)
        done.append(f"{name} ({n} funcs, {len(pieces)} rodata pieces)")
    print(f"sectionize-overlays: {len(done)} overlay(s): {', '.join(done) or 'none'}")
