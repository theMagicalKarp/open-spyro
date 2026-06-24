"""The C-match progress report.

Joins splat's function inventory with the seeded names (config/symbol_addrs.txt)
and the current build state, then emits two artifacts:

    PROGRESS.md    human report: per-segment summary + full function table (tracked)
    progress.json  machine-readable per-function records consumed by CI's PR
                   progress-delta comment. A generated build artifact — gitignored,
                   not committed.

The README badge is a self-contained static shields.io URL rewritten in place by
``update_readme_badge`` (no separate endpoint file).

Run via ``make progress``. Pure stdlib, no toolchain needed — so it is cheap and
deterministic in CI. Stable ordering (sorted by address) means no spurious diffs.

--- What counts as a function -----------------------------------------------
The worklist is the UNION of two sources, deduped by address:
  * config/symbol_addrs.txt `type:func` entries — the seeded named functions
    (the stable master; a function keeps its row here even after it is matched
    and its asm is removed from asm/text.s).
  * asm/text.s `glabel ... endlabel` blocks — picks up the auto-detected
    `func_xxxxxxxx` functions Ghidra never named, so the byte accounting covers
    the whole .text (not just the named subset). Without these the denominator
    would be ~54 KB short and the badge could falsely reach 100%.
Seeded names win over auto names at the same address.

--- Status -------------------------------------------------------------------
  asm      no C override exists and the asm body is active in asm/text.s. This is
           the baseline state for every function.
  matched  a C override (src/c/<name>.c) exists AND the rebuilt EXE bytes at its
           VMA equal the original (verified, not asserted). The override's asm is
           still present in text.s but suppressed by its `.ifndef HAVE_C_<name>`
           guard (--defsym at build) — so override PRESENCE, not asm absence, is
           the matched signal. (A legacy asm-removed-from-text.s path is still
           honored as matched if its bytes verify.)
  wip      C work in progress: an override exists but its bytes do not (yet) match
           / cannot be verified (no rebuilt EXE), or it is explicitly tagged
           `status:wip` in symbol_addrs.txt.
"""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any

from open_spyro.paths import repo_root

# README badge is rewritten in place between these markers (self-contained static
# shields.io badge — no remote/raw URL needed; regenerated on every `make progress`).
BADGE_START = "<!-- progress-badge -->"
BADGE_END = "<!-- /progress-badge -->"

SYM_FUNC = re.compile(r"^(\S+)\s*=\s*0x([0-9a-fA-F]+);.*\btype:func\b.*\bsize:0x([0-9a-fA-F]+)")
# A function's first instruction carries its VMA in the byte comment:
#   /* <rom> <vram8> <word8> */  <mnemonic ...>
INSTR_VRAM = re.compile(r"/\*\s*[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})\s+[0-9A-Fa-f]{8}\s*\*/")
# Any label that means "this function's asm lives in text.s".
DEFINED = re.compile(r"^\s*(?:glabel|alabel|dlabel|nonmatching)\s+(\S+?)(?:,|\s|$)")


def _segments(repo: Path) -> list[dict[str, Any]]:
    """Per-segment descriptors: main + one per overlay.

    lib_ranges: [vram_lo, vram_hi) windows holding stock Sony PSY-Q SDK / libc code
    that is statically linked into the EXE but is NOT ours to decompile. Functions
    in these windows are flagged `lib:true` and EXCLUDED from the headline / badge
    denominator. The main-EXE SDK tail starts at `_bu_init` (0x80068494) and runs
    contiguously to the end of .text. The game's own named memcard layer (MemCard*)
    sits ABOVE this and stays counted as game code.
    """
    segments: list[dict[str, Any]] = [
        {
            "name": "main",
            "symbol_addrs": repo / "config/symbol_addrs.txt",
            "asm": repo / "asm/text.s",
            "src_c": repo / "src/c",  # C overrides; file stem == function name
            "orig": repo / "disc/orig/SCUS_942.28",
            "rebuilt": repo / "build/main/SCUS_942.28",
            "vram_base": 0x80010000,
            "file_base": 0x800,  # rodata starts at file 0x800 == vram 0x80010000
            "lib_ranges": [(0x80068494, 0x8006BBE0)],
        },
    ]

    # Overlays: one segment per .ovl, appended from the frozen overlay table. Each
    # loads at the shared overlay VRAM; file_base is 0 because the .ovl file starts
    # exactly at that VRAM. No seeded names yet -> every function comes from its
    # asm/overlays/<name>/text.s as an unnamed func_xxxxxxxx (status asm). They are
    # NOT library code, so they count toward the game-code denominator.
    overlays_json = repo / "config/overlays.json"
    if overlays_json.exists():
        for r in json.loads(overlays_json.read_text())["overlays"]:
            name = r["name"]
            segments.append(
                {
                    "name": f"ovl/{name}",
                    "symbol_addrs": repo / "config/overlays" / f"{name}.symbols.txt",
                    "asm": repo / "asm/overlays" / name / "text.s",
                    "orig": repo / "disc/orig/overlays" / r["ovl_file"],
                    "rebuilt": repo / "build/overlays" / r["ovl_file"],
                    "vram_base": r["vram"],
                    "file_base": 0,
                    "lib_ranges": [],
                }
            )
    return segments


def parse_symbol_funcs(path: Path) -> dict[int, tuple[str, int, bool]]:
    """addr -> (name, size, explicit_wip) for every `type:func` in symbol_addrs."""
    funcs: dict[int, tuple[str, int, bool]] = {}
    if not path.exists():
        return funcs
    for line in path.read_text().splitlines():
        m = SYM_FUNC.match(line)
        if not m:
            continue
        addr = int(m.group(2), 16)
        wip = "status:wip" in line
        funcs[addr] = (m.group(1), int(m.group(3), 16), wip)
    return funcs


def parse_asm(path: Path) -> tuple[dict[int, tuple[str, int]], set[str]]:
    """Parse asm/text.s once.

    Returns (blocks, defined_names) where:
      blocks         addr -> (name, size) for each glabel..endlabel function body
                     (this is where the auto func_xxxxxxxx names come from).
      defined_names  set of every symbol defined in the file as a function label
                     (glabel/alabel/nonmatching/dlabel) — the "asm is still here"
                     test for status.
    """
    blocks: dict[int, tuple[str, int]] = {}
    defined: set[str] = set()
    if not path.exists():
        return blocks, defined
    cur: str | None = None
    start: int | None = None
    last: int | None = None
    for line in path.read_text().splitlines():
        dm = DEFINED.match(line)
        if dm:
            defined.add(dm.group(1))
        iv = INSTR_VRAM.search(line)
        if iv:
            last = int(iv.group(1), 16)
        gm = re.match(r"^glabel\s+(\S+)", line)
        if gm:
            cur = gm.group(1)
            start = None
        if cur and start is None and iv:
            start = int(iv.group(1), 16)
        em = re.match(r"^endlabel\s+(\S+)", line)
        if em and cur is not None and cur == em.group(1) and start is not None and last is not None:
            blocks[start] = (cur, last + 4 - start)
            cur = None
    return blocks, defined


def load_image(path: Path) -> bytes | None:
    return path.read_bytes() if path.exists() else None


def func_bytes(
    image: bytes | None, vram: int, size: int, vram_base: int, file_base: int
) -> bytes | None:
    if image is None:
        return None
    off = vram - vram_base + file_base
    if off < 0 or off + size > len(image):
        return None
    return image[off : off + size]


def load_overrides(path: Path | None) -> set[str]:
    """Set of function names with a C override (src/c/<name>.c -> <name>)."""
    if path is None or not Path(path).is_dir():
        return set()
    return {p.stem for p in Path(path).glob("*.c")}


def build_segment(seg: dict[str, Any]) -> list[dict[str, Any]]:
    sym = parse_symbol_funcs(seg["symbol_addrs"])
    blocks, defined = parse_asm(seg["asm"])
    overrides = load_overrides(seg.get("src_c"))

    # Universe: union by address, seeded names/sizes win over auto.
    universe: dict[int, list[Any]] = {}
    for addr, (name, size) in blocks.items():
        universe[addr] = [name, size, False]
    for addr, (name, size, wip) in sym.items():
        universe[addr] = [name, size, wip]

    orig = load_image(seg["orig"])
    rebuilt = load_image(seg["rebuilt"])
    # CI runs without the copyrighted disc, so seg["orig"] (disc/orig/...) is absent.
    # In that case use the rebuilt image as the matched-byte baseline. This is sound
    # *because CI runs `make verify` first*: that step asserts the whole rebuilt
    # artifact's SHA-1 equals the recorded original (config/sha1sums.txt /
    # overlays.json), i.e. rebuilt == original byte-for-byte. So per-function
    # rebuilt-vs-rebuilt confirms the same "asm removed and bytes still match" fact.
    if orig is None:
        orig = rebuilt
    lib_ranges = seg.get("lib_ranges", [])

    records = []
    for addr in sorted(universe):
        name, size, explicit_wip = universe[addr]
        if name in overrides:
            # A C override exists: matched iff its rebuilt bytes equal the original
            # (verified). Its guarded asm is still in text.s, so don't treat that as
            # "asm". A failing/unverifiable override is wip, not a false match.
            ob = func_bytes(orig, addr, size, seg["vram_base"], seg["file_base"])
            rb = func_bytes(rebuilt, addr, size, seg["vram_base"], seg["file_base"])
            status = "matched" if (ob is not None and ob == rb) else "wip"
        elif explicit_wip:
            status = "wip"
        elif name in defined:
            status = "asm"  # active asm body, no override -> not matched
        else:
            # Legacy path: asm removed from text.s without an override file. Confirm
            # the rebuilt bytes equal the original (verified).
            ob = func_bytes(orig, addr, size, seg["vram_base"], seg["file_base"])
            rb = func_bytes(rebuilt, addr, size, seg["vram_base"], seg["file_base"])
            status = "matched" if (ob is not None and ob == rb) else "wip"
        is_lib = any(lo <= addr < hi for lo, hi in lib_ranges)
        records.append(
            {
                "addr": f"0x{addr:08x}",
                "name": name,
                "segment": seg["name"],
                "size": size,
                "status": status,
                "matched_bytes": size if status == "matched" else 0,
                "lib": is_lib,
            }
        )
    return records


def shield_color(pct: float) -> str:
    if pct >= 100:
        return "brightgreen"
    if pct >= 75:
        return "green"
    if pct >= 50:
        return "yellowgreen"
    if pct >= 25:
        return "yellow"
    if pct > 0:
        return "orange"
    return "red"


def update_readme_badge(readme: Path, pct: float, color: str) -> None:
    """Rewrite the static shields.io badge between the README markers in place."""
    if not readme.exists():
        return
    text = readme.read_text()
    if BADGE_START not in text or BADGE_END not in text:
        return
    msg = f"{pct:.2f}%".replace("%", "%25")  # URL-encode for shields.io static badge
    badge = (
        f"{BADGE_START}\n"
        f"![matched](https://img.shields.io/badge/matched-{msg}-{color})\n"
        f"{BADGE_END}"
    )
    new = re.sub(
        re.escape(BADGE_START) + r".*?" + re.escape(BADGE_END),
        badge,
        text,
        flags=re.DOTALL,
    )
    if new != text:
        readme.write_text(new)


def _blank() -> dict[str, int]:
    return {"total": 0, "matched": 0, "wip": 0, "asm": 0, "total_bytes": 0, "matched_bytes": 0}


def seg_summary(records: list[dict[str, Any]]) -> dict[str, dict[str, dict[str, int]]]:
    """segment -> {"game": counts, "lib": counts}.

    The headline / badge are computed over `game` only; `lib` (stock PSY-Q/libc)
    is tracked separately and excluded.
    """
    out: dict[str, dict[str, dict[str, int]]] = {}
    for r in records:
        seg = out.setdefault(r["segment"], {"game": _blank(), "lib": _blank()})
        s = seg["lib"] if r["lib"] else seg["game"]
        s["total"] += 1
        s[r["status"]] += 1
        s["total_bytes"] += r["size"]
        s["matched_bytes"] += r["matched_bytes"]
    return out


def _pct(s: dict[str, int]) -> float:
    return (s["matched_bytes"] / s["total_bytes"] * 100) if s["total_bytes"] else 0.0


def render_md(records: list[dict[str, Any]], summary: dict[str, dict[str, dict[str, int]]]) -> str:
    game = {k: sum(seg["game"][k] for seg in summary.values()) for k in _blank()}
    lib = {k: sum(seg["lib"][k] for seg in summary.values()) for k in _blank()}
    pct = _pct(game)

    lines = []
    lines.append("# Progress — open-spyro C-match")
    lines.append("")
    lines.append(
        f"**Game code matched: {game['matched_bytes']:,} / {game['total_bytes']:,} "
        f"bytes ({pct:.2f}%)** — generated by `make progress` (`open-spyro progress`)."
    )
    lines.append("")
    lines.append(
        "Status: `matched` = verified byte-identical from C · "
        "`wip` = C in progress · `asm` = original assembly (`INCLUDE_ASM`)."
    )
    lines.append("")
    lines.append(
        f"Excluded from the percentage: **{lib['total']} library functions / "
        f"{lib['total_bytes']:,} bytes** of stock PSY-Q SDK + libc statically linked "
        f"into the EXE (tracked for completeness, but not ours to decompile)."
    )
    lines.append("")
    lines.append("## Per-segment summary (game code)")
    lines.append("")
    lines.append("| Segment | Functions | matched | wip | asm | Bytes matched | Total bytes | % |")
    lines.append("|---|--:|--:|--:|--:|--:|--:|--:|")
    for name in sorted(summary):
        s = summary[name]["game"]
        lines.append(
            f"| {name} | {s['total']} | {s['matched']} | {s['wip']} | {s['asm']} | "
            f"{s['matched_bytes']:,} | {s['total_bytes']:,} | {_pct(s):.2f}% |"
        )
    lines.append(
        f"| **all** | {game['total']} | {game['matched']} | {game['wip']} | "
        f"{game['asm']} | {game['matched_bytes']:,} | {game['total_bytes']:,} | {pct:.2f}% |"
    )
    lines.append("")
    lines.append("## Library / SDK (excluded from %)")
    lines.append("")
    lines.append("| Segment | Functions | Bytes |")
    lines.append("|---|--:|--:|")
    for name in sorted(summary):
        s = summary[name]["lib"]
        if s["total"]:
            lines.append(f"| {name} | {s['total']} | {s['total_bytes']:,} |")
    lines.append(f"| **all** | {lib['total']} | {lib['total_bytes']:,} |")
    lines.append("")
    lines.append("## Functions")
    lines.append("")
    lines.append("| Address | Name | Segment | Size | Status | Class |")
    lines.append("|---|---|---|--:|---|---|")
    for r in records:
        cls = "lib" if r["lib"] else "game"
        lines.append(
            f"| `{r['addr']}` | {r['name']} | {r['segment']} | {r['size']} | "
            f"{r['status']} | {cls} |"
        )
    lines.append("")
    return "\n".join(lines)


def run() -> None:
    repo = repo_root()
    out_json = repo / "progress.json"
    out_md = repo / "PROGRESS.md"
    readme = repo / "README.md"

    records: list[dict[str, Any]] = []
    for seg in _segments(repo):
        records.extend(build_segment(seg))
    records.sort(key=lambda r: (int(r["addr"], 16), r["segment"]))

    summary = seg_summary(records)
    # Headline / badge are GAME-only (stock SDK/libc excluded). `total_*` fields
    # below mean game code; library_* report the excluded SDK for full accounting.
    game = [r for r in records if not r["lib"]]
    lib = [r for r in records if r["lib"]]
    total_bytes = sum(r["size"] for r in game)
    matched_bytes = sum(r["matched_bytes"] for r in game)
    pct = (matched_bytes / total_bytes * 100) if total_bytes else 0.0

    out_json.write_text(
        json.dumps(
            {
                "total_functions": len(game),
                "total_code_bytes": total_bytes,
                "matched_bytes": matched_bytes,
                "matched_pct": round(pct, 4),
                "library_functions": len(lib),
                "library_code_bytes": sum(r["size"] for r in lib),
                "all_functions": len(records),
                "functions": records,
            },
            indent=2,
        )
        + "\n"
    )
    out_md.write_text(render_md(records, summary))
    color = shield_color(pct)
    update_readme_badge(readme, pct, color)

    print(
        f"progress: {len(game)} game functions, "
        f"{matched_bytes:,}/{total_bytes:,} bytes matched ({pct:.2f}%); "
        f"{len(lib)} library functions excluded ({sum(r['size'] for r in lib):,} bytes)"
    )
    print(f"  wrote {out_md.relative_to(repo)}, {out_json.relative_to(repo)}")
