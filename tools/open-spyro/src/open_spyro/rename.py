"""Byte-neutral symbol renames across the whole repo.

A symbol's name lives in many places at once: the splat seeds
(``config/symbol_addrs.txt``, ``config/overlays/<seg>.symbols.txt``,
``config/overlays/undefined_syms_manual.txt``), the asm (labels, references,
``.text.<Name>`` sections, ``HAVE_C_<Name>`` guards), the text-layout / slot
fragments, the headers, the C sources (and their *file names*), and the
generated reports. ``rename`` rewrites every one of them in a single step so a
build never sees a half-renamed symbol.

Names never reach the linked bytes, so a correct rename is byte-neutral — but
``make check`` stays the gate (a rename can still surface a declaration clash).

Scopes
------
``main``     symbol seeded in ``config/symbol_addrs.txt``, or an autolabel
             (``D_``/``func_``/``jtbl_`` + 8 hex) defined in main asm or not
             defined in any asm (``.bss``/hardware). Overlay asm and the
             splat-generated ``*.undefined_*.auto.txt`` keep the autolabel:
             ``make split`` regenerates those, and overlay links already
             resolve main names through ``config/symbol_addrs.txt``.
``<seg>``    symbol seeded in, or defined only by, one overlay. Only that
             overlay's asm/config/src is rewritten. An autolabel defined by
             several overlays (each its own object) needs ``--overlay``.

Durability
----------
An autolabel has no seed line, so ``make split`` would regenerate the old
name. ``rename`` appends a seed line for the new name to the scope's seed file,
so a re-split reproduces it.

Views
-----
Matching work spells some accesses through same-address aliases
(``D_800777EC_s``, ``g_nFoo_blk``). By default ``OLD_<suffix>`` is renamed to
``NEW_<suffix>`` alongside ``OLD``; ``--no-views`` disables that.

Merges (``canonicalize``)
-------------------------
Overlay C was decompiled against splat autolabels, so it still calls
``func_80017758`` where main calls ``AddVector``. When ``OLD`` is an autolabel
and ``NEW`` is already seeded at the same address, ``rename`` merges: only C
sources are rewritten (asm keeps its autolabel, no seed line is added).
``canonicalize`` applies every such merge it can find in ``src/``.
"""

from __future__ import annotations

import json
import re
import subprocess
from dataclasses import dataclass, field
from pathlib import Path

from open_spyro.paths import repo_root

AUTOLABEL = re.compile(r"^(?:func|D|jtbl)_([0-9A-F]{8})$")
_SEED = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;")
# Names that are placeholders, not names: splat autolabels and leftover Ghidra ones.
PLACEHOLDER = re.compile(r"^(?:(?:func|D|jtbl)_[0-9A-F]{8}|(?:FUN|DAT|LAB)_[0-9a-fA-F]{8})$")
_IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
_LABEL_DEF = r"^\s*(?:glabel|dlabel|jlabel|nonmatching)\s+{0}\b|^{0}:"
_AUTOLABEL_REF = re.compile(r"(?<![A-Za-z0-9_])(?:func|D)_([0-9A-F]{8})(?![A-Za-z0-9_])")

MAIN_SEED = Path("config/symbol_addrs.txt")
MANUAL_SEED = Path("config/overlays/undefined_syms_manual.txt")
SKIP_PREFIXES = ("ghidra_export/", ".github/")


@dataclass
class Plan:
    old: str
    new: str
    scope: str  # "main" or an overlay segment name
    merge: bool
    views: bool
    edits: dict[Path, tuple[str, int]] = field(default_factory=dict)  # path -> (text, hits)
    moves: list[tuple[Path, Path]] = field(default_factory=list)
    seed: tuple[Path, str] | None = None


def _tracked(root: Path) -> list[Path]:
    out = subprocess.run(
        ["git", "ls-files", "-z"], cwd=root, check=True, capture_output=True
    ).stdout.decode()
    return [Path(p) for p in out.split("\0") if p and not p.startswith(SKIP_PREFIXES)]


def _read(root: Path, rel: Path) -> str | None:
    try:
        return (root / rel).read_text()
    except (UnicodeDecodeError, FileNotFoundError, IsADirectoryError):
        return None


def seeds(root: Path) -> dict[str, tuple[int, Path]]:
    """name -> (address, seed file) across every hand-maintained seed file."""
    ovl = sorted(p.relative_to(root) for p in (root / "config/overlays").glob("*.symbols.txt"))
    files = [MAIN_SEED, MANUAL_SEED, *ovl]
    out: dict[str, tuple[int, Path]] = {}
    for rel in files:
        text = _read(root, rel)
        if text is None:
            continue
        for line in text.splitlines():
            m = _SEED.match(line.split("//", 1)[0])
            if m:
                out.setdefault(m.group(1), (int(m.group(2), 16), rel))
    return out


def _seg_of(rel: Path) -> str | None:
    """Overlay segment a path belongs to, or None for main/global files."""
    parts = rel.parts
    if parts[:2] in (("asm", "overlays"), ("src", "overlays")) and len(parts) > 2:
        return parts[2]
    if parts[:2] == ("config", "overlays") and rel != MANUAL_SEED:
        return rel.name.split(".", 1)[0]
    return None


def _defining_segments(root: Path, name: str, files: list[Path]) -> set[str]:
    """Segments ("main" or overlay) whose asm defines ``name`` as a label."""
    pat = re.compile(_LABEL_DEF.format(re.escape(name)), re.M)
    segs: set[str] = set()
    for rel in files:
        if rel.parts[0] != "asm" or rel.suffix != ".s":
            continue
        text = _read(root, rel)
        if text and name in text and pat.search(text):
            segs.add(_seg_of(rel) or "main")
    return segs


def _pattern(old: str, views: bool) -> re.Pattern[str]:
    # HAVE_C_<Name> guards glue the name onto an identifier prefix; allow that one.
    suffix = r"(_[A-Za-z0-9]+)?" if views else r"()"
    return re.compile(
        rf"(?:(?<=HAVE_C_)|(?<![A-Za-z0-9_])){re.escape(old)}{suffix}(?![A-Za-z0-9_])"
    )


def _in_scope(rel: Path, scope: str, merge: bool) -> bool:
    if merge:
        return rel.parts[0] == "src"
    seg = _seg_of(rel)
    if scope == "main":
        # Overlay asm + splat's undefined_*.auto.txt keep main autolabels (see module doc).
        if seg is None:
            return True
        return not (rel.parts[0] == "asm" or rel.name.endswith(".auto.txt"))
    return seg in (None, scope)


def plan(
    old: str, new: str, *, overlay: str = "", views: bool = True, merge_ok: bool = True
) -> Plan:
    root = repo_root()
    if not _IDENT.match(new):
        raise SystemExit(f"rename: {new!r} is not a C identifier")
    if old == new:
        raise SystemExit("rename: old and new names are identical")
    files = _tracked(root)
    seeded = seeds(root)
    auto = AUTOLABEL.match(old)

    merge = False
    if new in seeded:
        if merge_ok and auto and seeded[new][0] == int(auto.group(1), 16):
            merge = True
        else:
            raise SystemExit(f"rename: {new} already exists ({seeded[new][1]})")

    seed: tuple[Path, str] | None = None
    if merge:
        scope = "main"
    elif old in seeded:
        sfile = seeded[old][1]
        scope = "main" if sfile == MANUAL_SEED else (_seg_of(sfile) or "main")
    elif auto:
        segs = _defining_segments(root, old, files)
        if overlay:
            if overlay not in segs:
                raise SystemExit(f"rename: {old} is not defined in overlay {overlay}")
            scope = overlay
        elif len(segs) > 1:
            raise SystemExit(
                f"rename: {old} is a distinct object in {sorted(segs)}; pick one with --overlay"
            )
        else:
            scope = next(iter(segs), "main")
        seed = _seed_line(root, old, new, scope, auto.group(1))
    else:
        raise SystemExit(f"rename: {old} is not seeded and is not a splat autolabel")

    p = Plan(old, new, scope, merge, views and not merge, seed=seed)
    pat = _pattern(old, p.views)
    clash = _pattern(new, False)
    for rel in files:
        if not _in_scope(rel, scope, merge):
            continue
        # Per-function sources are named after their function (src/c/<Name>.c[.wip]),
        # including comment-only fragment stubs that never spell the name inside.
        stem, dot, ext = rel.name.partition(".")
        if rel.parts[0] == "src" and stem == old and dot and not merge:
            p.moves.append((rel, rel.with_name(f"{new}.{ext}")))
        text = _read(root, rel)
        if text is None or old not in text:
            continue
        if not merge and clash.search(text):
            raise SystemExit(f"rename: {new} already appears in {rel}")
        out, n = pat.subn(lambda m: new + (m.group(1) or ""), text)
        if n:
            p.edits[rel] = (out, n)
    if not p.edits and not p.moves:
        raise SystemExit(f"rename: no references to {old} found")
    return p


def _seed_line(root: Path, old: str, new: str, scope: str, hexaddr: str) -> tuple[Path, str]:
    target = MAIN_SEED if scope == "main" else Path(f"config/overlays/{scope}.symbols.txt")
    kind = "func" if old.startswith("func_") else "data"
    size = ""
    if kind == "func":
        layout = Path("config/text_layout.json")
        if scope != "main":
            layout = Path(f"config/overlays/{scope}.text_layout.json")
        text = _read(root, layout)
        if text:
            for fn in json.loads(text).get("functions", []):
                if fn["name"] == old:
                    size = f" size:0x{fn['size']:x}"
    return target, f"{new} = 0x{hexaddr}; // type:{kind}{size}"


def apply(p: Plan) -> None:
    root = repo_root()
    for rel, (text, _) in p.edits.items():
        (root / rel).write_text(text)
    if p.seed:
        target, line = p.seed
        path = root / target
        body = path.read_text()
        path.write_text(body + ("" if body.endswith("\n") else "\n") + line + "\n")
    for src, dst in p.moves:
        if (root / dst).exists():
            raise SystemExit(f"rename: {dst} already exists")
        subprocess.run(["git", "mv", str(src), str(dst)], cwd=root, check=True)


def report(p: Plan, dry_run: bool) -> None:
    verb = "would rewrite" if dry_run else "rewrote"
    kind = "merge" if p.merge else f"scope {p.scope}"
    total = sum(n for _, n in p.edits.values())
    print(f"rename {p.old} -> {p.new} ({kind}): {verb} {total} refs in {len(p.edits)} files")
    for rel, (_, n) in sorted(p.edits.items()):
        print(f"  {n:5d}  {rel}")
    for src, dst in p.moves:
        print(f"  move   {src} -> {dst}")
    if p.seed:
        print(f"  seed   {p.seed[0]}: {p.seed[1]}")
    if not dry_run:
        print("next: make check (must report 0 diffs before committing)")


def run(old: str, new: str, overlay: str, views: bool, dry_run: bool) -> None:
    p = plan(old, new, overlay=overlay, views=views)
    if not dry_run:
        apply(p)
    report(p, dry_run)


def alias_candidates(root: Path, rels: list[Path]) -> dict[Path, dict[str, str]]:
    """Per C file: autolabel refs whose address main already names (old -> canonical)."""
    by_addr: dict[int, str] = {}
    for name, (addr, sfile) in seeds(root).items():
        if sfile == MAIN_SEED and not PLACEHOLDER.match(name):
            by_addr.setdefault(addr, name)
    out: dict[Path, dict[str, str]] = {}
    for rel in rels:
        text = _read(root, rel)
        if not text:
            continue
        hits = {
            m.group(0): by_addr[int(m.group(1), 16)]
            for m in _AUTOLABEL_REF.finditer(text)
            if int(m.group(1), 16) in by_addr
        }
        if hits:
            out[rel] = hits
    return out


def canonicalize(paths: list[str], dry_run: bool) -> None:
    """Rewrite autolabel aliases of already-named main symbols to the canonical name."""
    root = repo_root()
    rels = [p for p in _tracked(root) if p.parts[0] == "src" and p.suffix == ".c"]
    if paths:
        want = [Path(p) for p in paths]
        rels = [r for r in rels if any(r == w or w in r.parents for w in want)]
    cands = alias_candidates(root, rels)
    total = 0
    for rel, hits in sorted(cands.items()):
        text = (root / rel).read_text()
        n = 0
        for old, new in hits.items():
            text, k = _pattern(old, False).subn(new, text)
            n += k
        total += n
        if not dry_run:
            (root / rel).write_text(text)
        names = ", ".join(f"{o}->{c}" for o, c in sorted(hits.items()))
        print(f"  {n:4d}  {rel}: {names}")
    verb = "would rewrite" if dry_run else "rewrote"
    print(f"canonicalize: {verb} {total} refs in {len(cands)} files")
    if cands and not dry_run:
        print("next: make check — a clash with a header declaration shows up as a compile error")
