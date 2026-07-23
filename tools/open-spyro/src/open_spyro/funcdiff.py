"""One-command per-function byte diff — the match loop's inner iteration.

``open-spyro diff <Func>`` rebuilds the containing artifact (main EXE or a
function-split overlay), slices the function's bytes at its VMA out of both the
rebuilt file and the original, and prints an instruction-level side-by-side of
any difference plus a match percentage. Exit 0 = byte-identical, 1 = differs,
2 = cannot run (unknown function, missing disc/orig, build failure).

Everything binary (build, slice, compare, disassemble) runs inside ONE
``tools/docker_env.sh`` invocation: reading build output from the host right
after a docker build can serve stale bytes through the volume mount — a
documented false-match trap. Only text crosses back to the host.

If the function has no ``src/c/<Name>.c`` but does have a parked
``<Name>.c.wip``, the whole-EXE build is skipped in favor of ``_run_wip``: it
compiles the wip candidate and the function's original asm block as
standalone objects (same trick as ``partial.py``, since a non-fitting wip
can't be linked into the EXE without shifting every fixed-VMA slot after it —
see ``gen_slots_ld.py``) and diffs them instruction-by-instruction, reloc
included since neither side is linked yet. Exit codes mirror the normal path
(0 = instructions match, 1 = differs) but this never proves a byte-identical
match — only that promoting the wip would be worth trying.
"""

from __future__ import annotations

import difflib
import json
import os
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

from open_spyro.overlays import load as load_overlays
from open_spyro.paths import repo_root

MAIN_VMA_BASE = 0x80010000
MAIN_FILE_HEADER = 0x800

_INSN_RE = re.compile(r"^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(.*)$")

# Parallel objdump line shapes for the wip (unlinked-object) path: instructions
# still carry unresolved relocations, printed on their own trailing line.
_RELOC_INSN_RE = re.compile(r"^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(\S+)(.*)$")
_RELOC_RE = re.compile(r"^\s*[0-9a-f]+:\s+(R_MIPS_\S+)\s+(\S+)")

# ANSI SGR codes used in the side-by-side listing.
_RED = "31"  # original (expected) bytes
_GREEN = "32"  # rebuilt (actual) bytes
_BOLD_RED = "1;31"
_BOLD_GREEN = "1;32"


def _want_color(mode: str) -> bool:
    """Resolve --color=auto/always/never against TTY + the NO_COLOR convention."""
    if mode == "always":
        return True
    if mode == "never":
        return False
    return sys.stdout.isatty() and not os.environ.get("NO_COLOR")


def _paint(text: str, code: str, on: bool) -> str:
    return f"\033[{code}m{text}\033[0m" if on else text


@dataclass
class _Target:
    name: str
    segment: str  # "main" or the overlay name
    vma: int
    size: int
    built: str  # repo-relative, used inside the container
    orig: str
    file_off: int
    build_cmd: str


def _layout_names(layout: Path) -> list[dict]:
    return json.loads(layout.read_text())["functions"]


def _locate(repo: Path, name: str) -> _Target | None:
    for fn in _layout_names(repo / "config/text_layout.json"):
        if fn["name"] == name:
            return _Target(
                name=name,
                segment="main",
                vma=fn["vram"],
                size=fn["size"],
                built="build/main/SCUS_942.28",
                orig="disc/orig/SCUS_942.28",
                file_off=MAIN_FILE_HEADER + fn["vram"] - MAIN_VMA_BASE,
                build_cmd="bash tools/build_main.sh",
            )
    for ovl in load_overlays():
        layout = repo / "config/overlays" / f"{ovl['name']}.text_layout.json"
        if not layout.is_file():
            continue
        for fn in _layout_names(layout):
            if fn["name"] == name:
                return _Target(
                    name=name,
                    segment=ovl["name"],
                    vma=fn["vram"],
                    size=fn["size"],
                    built=f"build/overlays/{ovl['name']}.ovl",
                    orig=f"disc/orig/overlays/{ovl['name']}.ovl",
                    file_off=fn["vram"] - ovl["vram"],
                    build_cmd=f"bash tools/build_overlays.sh {shlex.quote(ovl['name'])}",
                )
    return None


def _all_names(repo: Path) -> list[str]:
    names = [f["name"] for f in _layout_names(repo / "config/text_layout.json")]
    for ovl in load_overlays():
        layout = repo / "config/overlays" / f"{ovl['name']}.text_layout.json"
        if layout.is_file():
            names += [f["name"] for f in _layout_names(layout)]
    return names


def _container_script(t: _Target, build: bool) -> str:
    """Build (optionally), slice both files at the VMA, cmp, objdump on mismatch."""
    a, b = f"build/diff/{t.name}.orig.bin", f"build/diff/{t.name}.built.bin"
    objdump = f"mips-linux-gnu-objdump -D -b binary -m mips:3000 -EL --adjust-vma=0x{t.vma:x}"
    lines = ["set -euo pipefail", "mkdir -p build/diff"]
    if build:
        lines.append(
            f"{t.build_cmd} > build/diff/build.log 2>&1"
            " || { echo @@BUILDFAIL@@; cat build/diff/build.log; exit 0; }"
        )
    # head-then-tail (not tail|head): head closing early would SIGPIPE tail and
    # trip pipefail.
    lines += [
        f"head -c {t.file_off + t.size} {t.orig} | tail -c {t.size} > {a}",
        f"head -c {t.file_off + t.size} {t.built} | tail -c {t.size} > {b}",
        f"if cmp -s {a} {b}; then echo @@IDENTICAL@@; else",
        f"  echo @@ORIG@@; {objdump} {a}",
        f"  echo @@BUILT@@; {objdump} {b}",
        "fi",
    ]
    return "\n".join(lines)


def _parse_insns(text: str) -> list[tuple[int, str, str]]:
    """objdump lines -> (vma, wordhex, mnemonic-text)."""
    out = []
    for line in text.splitlines():
        m = _INSN_RE.match(line)
        if m:
            out.append((int(m.group(1), 16), m.group(2), m.group(3).strip().expandtabs(8)))
    return out


def _print_diff(
    orig: list[tuple[int, str, str]],
    built: list[tuple[int, str, str]],
    full: bool,
    color: bool = False,
) -> int:
    """Side-by-side listing; returns the number of differing instructions."""
    rows = list(zip(orig, built, strict=True))
    bad = [i for i, (a, b) in enumerate(rows) if a[1] != b[1]]
    show: set[int] = set(range(len(rows)))
    if not full and len(rows) > 60:
        show = {j for i in bad for j in range(max(0, i - 2), min(len(rows), i + 3))}
    width = max((len(a[2]) for a, _ in rows), default=0)
    width = min(width, 44)
    last = -1
    for i, (a, b) in enumerate(rows):
        if i not in show:
            continue
        if i != last + 1:
            print("  ...")
        last = i
        # Pad the left cell before painting so ANSI codes don't skew alignment.
        left = f"{a[1]}  {a[2]:<{width}}"
        if a[1] == b[1]:
            print(f"   {a[0]:08x}  {left}")
        else:
            mark = _paint("!", _BOLD_RED, color)
            left = _paint(left, _RED, color)
            right = _paint(f"{b[1]}  {b[2]}", _GREEN, color)
            print(f" {mark} {a[0]:08x}  {left}   | {right}")
    return len(bad)


def _norm_sym(sym: str) -> str:
    """Collapse intra-`.text` self-references to one token (see partial.py)."""
    return "<LOCAL>" if sym.startswith(".text") else sym.split("+", 1)[0]


@dataclass
class _RInsn:
    word: str
    text: str  # mnemonic + operands, for display
    reloc: tuple[str, str] | None


def _parse_reloc_insns(text: str) -> list[_RInsn]:
    """`objdump -drz` output (unlinked object) -> per-instruction word/text/reloc."""
    out: list[_RInsn] = []
    for line in text.splitlines():
        m = _RELOC_INSN_RE.match(line)
        if m:
            disp = (m.group(3) + m.group(4)).strip().expandtabs(8)
            out.append(_RInsn(word=m.group(2), text=disp, reloc=None))
            continue
        r = _RELOC_RE.match(line)
        if r and out:
            out[-1].reloc = (r.group(1), _norm_sym(r.group(2)))
    return out


def _wip_container_script(mode: str, target_s: str, wip_c: str, target_o: str, cand_o: str) -> str:
    """Assemble the target's original asm block + compile the .wip candidate,
    standalone (not linked into the EXE — see partial.py for why), then dump
    both as relocatable objects for a reloc-aware instruction compare.
    """
    objdump = "mips-linux-gnu-objdump -drz -m mips:3000"
    return "\n".join(
        [
            "set -uo pipefail",
            "mkdir -p build/diff",
            "source config/compile.sh",
            f'"$AS" $AS_FLAGS -o {target_o} {shlex.quote(target_s)} > build/diff/asm.log 2>&1'
            " || { echo @@ASMFAIL@@; cat build/diff/asm.log; exit 0; }",
            f"rm -f {cand_o}",
            f"config/compile.sh {mode} {shlex.quote(wip_c)} {cand_o} "
            "> build/diff/compile.log 2>&1"
            " || { echo @@COMPILEFAIL@@; cat build/diff/compile.log; exit 0; }",
            f"echo @@TARGET@@; {objdump} {target_o}",
            f"echo @@CAND@@; {objdump} {cand_o}",
        ]
    )


def _print_reloc_diff(
    target: list[_RInsn], cand: list[_RInsn], full: bool, color: bool = False
) -> int:
    """Side-by-side listing for the wip (unlinked) path; returns bad-instruction count.

    Index-aligned like `_print_diff`, but a candidate that ran long or short just
    reads as trailing mismatches/blanks instead of raising (the two objects are
    not guaranteed to be the same length — that mismatch IS the interesting case).
    """
    n = max(len(target), len(cand))
    bad = [
        i
        for i in range(n)
        if i >= len(target)
        or i >= len(cand)
        or target[i].word != cand[i].word
        or target[i].reloc != cand[i].reloc
    ]
    show: set[int] = set(range(n))
    if not full and n > 60:
        show = {j for i in bad for j in range(max(0, i - 2), min(n, i + 3))}
    width = min(max((len(t.text) for t in target), default=0), 44)
    last = -1
    for i in range(n):
        if i not in show:
            continue
        if i != last + 1:
            print("  ...")
        last = i
        a = target[i] if i < len(target) else None
        b = cand[i] if i < len(cand) else None
        a_disp = f"{a.word}  {a.text:<{width}}" if a else "-- missing --".ljust(width + 10)
        b_disp = f"{b.word}  {b.text}" if b else "-- missing --"
        if i not in bad:
            print(f"   {i:3d}  {a_disp}")
        else:
            mark = _paint("!", _BOLD_RED, color)
            print(
                f" {mark} {i:3d}  {_paint(a_disp, _RED, color)}   | {_paint(b_disp, _GREEN, color)}"
            )
    return len(bad)


def _run_wip(t: _Target, wip: Path, full: bool, use_color: bool) -> None:
    """Compare a parked `.c.wip` candidate against its target asm, standalone.

    `.c.wip` files are excluded from the whole-EXE build by design (a
    non-matching size would shift every fixed-VMA slot after it — the failure
    this command exists to catch), so there is no linked binary to slice. This
    mirrors `partial.py`'s approach instead: assemble the function's original
    asm block and compile the wip candidate as free-standing objects, then
    diff them instruction-by-instruction (word + relocation, since neither
    side is linked to real addresses yet).
    """
    # Lazy: partial.py imports `_locate` from this module at top level, so a
    # module-level import here would be circular.
    from open_spyro.partial import _TARGET_PREAMBLE, _extract_block

    repo = repo_root()
    mode = "main" if t.segment == "main" else "ovl"
    asm_path = (
        repo / "asm/text.s" if t.segment == "main" else repo / "asm/overlays" / t.segment / "text.s"
    )
    if not asm_path.is_file():
        print(f"diff: missing {asm_path.relative_to(repo)}", file=sys.stderr)
        raise SystemExit(2)
    block = _extract_block(asm_path.read_text(), t.name)
    if block is None:
        print(f"diff: could not find {t.name}'s original asm block in {asm_path}", file=sys.stderr)
        raise SystemExit(2)

    diff_dir = repo / "build/diff"
    diff_dir.mkdir(parents=True, exist_ok=True)
    target_s = diff_dir / f"{t.name}.wip_target.s"
    target_s.write_text(_TARGET_PREAMBLE + block)

    target_o = f"build/diff/{t.name}.wip_target.o"
    cand_o = f"build/diff/{t.name}.wip_cand.o"
    script = _wip_container_script(
        mode, str(target_s.relative_to(repo)), str(wip.relative_to(repo)), target_o, cand_o
    )
    proc = subprocess.run(
        ["bash", "tools/docker_env.sh", "bash", "-c", script],
        cwd=repo,
        capture_output=True,
        text=True,
    )
    out = proc.stdout
    if proc.returncode != 0 or "@@ASMFAIL@@" in out or "@@COMPILEFAIL@@" in out:
        body = out
        for marker in ("@@ASMFAIL@@", "@@COMPILEFAIL@@"):
            if marker in out:
                body = out.split(marker, 1)[-1]
        print(body.strip(), file=sys.stderr)
        print(proc.stderr.strip(), file=sys.stderr)
        print(f"diff: build failed for {t.name} ({wip.name})", file=sys.stderr)
        raise SystemExit(2)

    target_txt, cand_txt = out.split("@@TARGET@@", 1)[1].split("@@CAND@@", 1)
    target_i, cand_i = _parse_reloc_insns(target_txt), _parse_reloc_insns(cand_txt)
    head = (
        f"{t.name} ({t.segment} @ 0x{t.vma:08x}, {t.size} bytes, {wip.relative_to(repo)}, "
        f"unlinked object compare — no real addresses yet)"
    )
    if len(target_i) == len(cand_i) and all(
        a.word == b.word and a.reloc == b.reloc for a, b in zip(target_i, cand_i, strict=True)
    ):
        print(f"{_paint('WIP-MATCH', _BOLD_GREEN, use_color)} {head}")
        print("       (instructions match — still parked; promote by dropping the .wip suffix)")
        return

    print(f"{_paint('WIP-DIFFER', _BOLD_RED, use_color)} {head}")
    n_bad = _print_reloc_diff(target_i, cand_i, full, use_color)
    total = len(target_i)
    n_extra = max(0, len(cand_i) - len(target_i))
    pct = 100.0 * (total - n_bad) / total if total else 0.0
    tag = _paint("WIP-DIFFER", _BOLD_RED, use_color)
    size_note = (
        f", candidate has {n_extra} extra instruction(s)"
        if len(cand_i) > len(target_i)
        else f", candidate is {total - len(cand_i)} instruction(s) short"
        if len(cand_i) < len(target_i)
        else ""
    )
    print(f"{tag} {n_bad}/{total} instructions differ ({pct:.1f}% match){size_note}")
    raise SystemExit(1)


def run(name: str, build: bool = True, full: bool = False, color: str = "auto") -> None:
    use_color = _want_color(color)
    repo = repo_root()
    t = _locate(repo, name)
    if t is None:
        close = difflib.get_close_matches(name, _all_names(repo), n=5, cutoff=0.5)
        hint = f" (close: {', '.join(close)})" if close else ""
        print(f"diff: unknown function '{name}'{hint}", file=sys.stderr)
        raise SystemExit(2)
    if not (repo / t.orig).is_file():
        print(f"diff: missing {t.orig} — run 'make extract' first", file=sys.stderr)
        raise SystemExit(2)

    rel = f"src/c/{t.name}.c" if t.segment == "main" else f"src/overlays/{t.segment}/{t.name}.c"
    override = repo / rel
    if not override.is_file():
        wip = override.with_name(override.name + ".wip")
        if wip.is_file():
            _run_wip(t, wip, full, use_color)
            return

    proc = subprocess.run(
        ["bash", "tools/docker_env.sh", "bash", "-c", _container_script(t, build)],
        cwd=repo,
        capture_output=True,
        text=True,
    )
    out = proc.stdout
    if proc.returncode != 0 or "@@BUILDFAIL@@" in out:
        body = out.split("@@BUILDFAIL@@", 1)[-1] if "@@BUILDFAIL@@" in out else out
        print(body.strip(), file=sys.stderr)
        print(proc.stderr.strip(), file=sys.stderr)
        print(f"diff: build failed for {t.name}", file=sys.stderr)
        raise SystemExit(2)

    src_note = rel if override.is_file() else "no C override (asm baseline)"
    head = f"{t.name} ({t.segment} @ 0x{t.vma:08x}, {t.size} bytes, {src_note})"

    if "@@IDENTICAL@@" in out:
        print(f"{_paint('MATCH', _BOLD_GREEN, use_color)}  {head}")
        if not override.is_file():
            print("       (nothing was overridden — this only proves the baseline)")
        return

    orig_txt, built_txt = out.split("@@ORIG@@", 1)[1].split("@@BUILT@@", 1)
    orig_i, built_i = _parse_insns(orig_txt), _parse_insns(built_txt)
    print(f"{_paint('DIFFER', _BOLD_RED, use_color)} {head}")
    n_bad = _print_diff(orig_i, built_i, full, use_color)
    total = len(orig_i)
    pct = 100.0 * (total - n_bad) / total if total else 0.0
    tag = _paint("DIFFER", _BOLD_RED, use_color)
    print(f"{tag} {n_bad}/{total} instructions differ ({pct:.1f}% match)")
    raise SystemExit(1)
