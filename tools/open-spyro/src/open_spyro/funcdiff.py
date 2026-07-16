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

    rel = f"src/c/{t.name}.c" if t.segment == "main" else f"src/overlays/{t.segment}/{t.name}.c"
    override = repo / rel
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
