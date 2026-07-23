"""Ranked triage of the remaining unmatched functions.

Joins the per-function progress records (``open_spyro.progress``) with signals
read straight from the splat asm — blocker signatures the locked toolchain can
never emit, shape hints that predict an easy match, and cross-overlay clone
fingerprints — plus a persistent per-function verdict ledger
(``config/triage_overrides.yaml``) recording every wall/park/attempt so no
function is ever re-inspected from scratch.

Emits ``TRIAGE.md`` (tracked): a ranked candidate table for fresh-match work,
the parked ``.c.wip`` harvest inventory, and the auto-skip list with reasons.

Run via ``make triage``. Pure stdlib, disc-free, deterministic (stable sort).

--- Blocker signatures (auto-skip; classes reference the cookbook skip list) ---
  %gp_rel operand        global accessed via $gp — our externs always expand to
                         lui/%lo, so no C form can match.
  lui $at, <literal>     literal MMIO address load ($at without a %hi reloc).
  jr $tN                 BIOS A0/B0/C0 syscall trampoline / kernel thunk — gcc
                         always appends its own `jr $ra` epilogue.
  switchdata reference   dense-switch jump table; the main EXE rejects loadable
                         .rodata in overrides (overlays are fine, so this only
                         skips main-segment functions).
  handwritten tag        $at-as-data functions (progress classification).

--- Verdict ledger -----------------------------------------------------------
``config/triage_overrides.yaml`` — one entry per function, append-only from
match sessions. A constrained YAML subset parsed here without a YAML dep:

    FunctionName:
      verdict: wall            # wall | parked | attempted | viable
      class: "B9 clamp"        # cookbook wall / tie class
      date: 2026-07-23
      note: "free text"

``wall`` removes the function from the candidate list permanently; ``parked``
routes it to the harvest inventory; ``attempted`` keeps it listed with a score
penalty; ``viable`` boosts it (a human vouched for it).
"""

from __future__ import annotations

import hashlib
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from open_spyro import progress
from open_spyro.paths import repo_root

# /* <rom> <vram8> <word8> */  <mnemonic> <operands...>
_INSTR = re.compile(r"/\*\s*[0-9A-Fa-f]+\s+[0-9A-Fa-f]{8}\s+([0-9A-Fa-f]{8})\s*\*/\s+(\S+)\s*(.*)$")
_JR_TRAMPOLINE = re.compile(r"^\$t\d")
_BRANCHES = re.compile(r"^b(?:eq|ne|lez|gtz|ltz|gez|eqz|nez)")


@dataclass
class Body:
    """One function's instruction stream, as read from the splat asm."""

    n_instr: int = 0
    n_jal: int = 0
    n_branch: int = 0
    callees: list[str] = field(default_factory=list)
    gp_rel: bool = False
    at_literal: bool = False
    trampoline: bool = False
    jr_table: bool = False
    words: list[str] = field(default_factory=list)

    @property
    def fingerprint(self) -> str:
        return hashlib.md5(" ".join(self.words).encode()).hexdigest()[:10]


def parse_bodies(path: Path) -> dict[str, Body]:
    """name -> Body for every glabel..endlabel block in one text.s."""
    bodies: dict[str, Body] = {}
    if not path.exists():
        return bodies
    cur: Body | None = None
    for line in path.read_text().splitlines():
        gm = re.match(r"^glabel\s+(\S+)", line)
        if gm:
            cur = bodies.setdefault(gm.group(1), Body())
            continue
        if re.match(r"^endlabel\s", line):
            cur = None
            continue
        if cur is None:
            continue
        im = _INSTR.search(line)
        if not im:
            continue
        word, mnem, ops = im.groups()
        ops = ops.split("/*", 1)[0].strip()  # drop trailing comments
        cur.n_instr += 1
        cur.words.append(word)
        if "%gp_rel" in ops:
            cur.gp_rel = True
        if mnem == "lui" and ops.startswith("$at") and "%hi" not in ops:
            cur.at_literal = True
        if mnem == "jr":
            if _JR_TRAMPOLINE.match(ops.lstrip()):
                cur.trampoline = True
            elif not ops.lstrip().startswith("$ra"):
                cur.jr_table = True
        if "switchdata" in ops or "jtbl" in ops:
            cur.jr_table = True
        if _BRANCHES.match(mnem):
            cur.n_branch += 1
        if mnem == "jal":
            cur.n_jal += 1
            cur.callees.append(ops.split()[0] if ops else "?")
    return bodies


def parse_overrides(path: Path) -> dict[str, dict[str, str]]:
    """Parse the constrained-YAML verdict ledger (see module docstring)."""
    entries: dict[str, dict[str, str]] = {}
    if not path.exists():
        return entries
    cur: dict[str, str] | None = None
    for raw in path.read_text().splitlines():
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue
        if not raw[0].isspace() and line.endswith(":"):
            cur = entries.setdefault(line[:-1].strip(), {})
            continue
        if cur is not None and ":" in line:
            k, v = line.split(":", 1)
            cur[k.strip()] = v.strip().strip("\"'")
    return entries


def _blockers(rec: dict[str, Any], body: Body | None) -> list[str]:
    """Cookbook-referenced auto-skip classes detected for one asm function."""
    out: list[str] = []
    if rec["handwritten"]:
        out.append("B6 handwritten")
    if "_OBJ_" in rec["name"]:
        out.append("B5 split-label fragment")
    if body is None:
        return out
    if body.gp_rel:
        out.append("B1 %gp_rel")
    if body.at_literal:
        out.append("B2 $at literal HW load")
    if body.trampoline:
        out.append("B3 BIOS trampoline")
    if body.jr_table and rec["segment"] == "main":
        out.append("B8 jr-table switch (main)")
    return out


def _score(
    rec: dict[str, Any], body: Body | None, ov: dict[str, str], clone: str
) -> tuple[int, list[str]]:
    """Heuristic viability score (higher = attempt sooner) + human-readable reasons."""
    why: list[str] = []
    score = 100 - min(rec["size"], 4000) // 50
    if rec["segment"] != "main":
        score += 10
        why.append("overlay (no -g3)")
    if clone == "matched":
        score += 40
        why.append("clone of a MATCHED function")
    elif clone == "family":
        score += 15
        why.append("clone family ≥3 (one recipe pays N×)")
    if rec.get("neighbor_matched"):
        score += 15
        why.append("matched neighbor in segment")
    if body and body.n_instr:
        if body.n_jal / body.n_instr >= 0.10 and body.n_jal >= 3:
            score += 10
            why.append(f"jal-chain shape ({body.n_jal} calls)")
        if body.n_branch / body.n_instr >= 0.20:
            score -= 10
            why.append("branch-dense")
    if rec["asm_hint"]:
        score -= 25
        why.append("asm-hint (GTE/handwritten fragments)")
    if rec["size"] > 2000:
        score -= 30
        why.append("megafunction (all-or-nothing)")
    if ov.get("verdict") == "attempted":
        score -= 20
        why.append(
            f"previously attempted ({ov.get('date', '?')}: {ov.get('class', ov.get('note', ''))})"
        )
    if ov.get("verdict") == "viable":
        score += 25
        why.append(f"hand-marked viable: {ov.get('note', '')}")
    return score, why


def collect(repo: Path) -> tuple[list[dict[str, Any]], dict[str, Body]]:
    """All progress records + parsed bodies, with clone/neighbor annotations."""
    partial = progress.load_partial(repo)
    records: list[dict[str, Any]] = []
    bodies: dict[str, Body] = {}
    for seg in progress._segments(repo):
        seg_records = progress.build_segment(seg, partial)
        seg_bodies = parse_bodies(seg["asm"])
        # Matched-neighbor annotation (previous/next game function in the segment).
        ordered = sorted(seg_records, key=lambda r: int(r["addr"], 16))
        for i, r in enumerate(ordered):
            near = ordered[max(0, i - 1) : i] + ordered[i + 1 : i + 2]
            r["neighbor_matched"] = any(n["status"] == "matched" for n in near)
        records.extend(seg_records)
        bodies.update(seg_bodies)
    return records, bodies


def clone_class(
    rec: dict[str, Any], body: Body | None, fp_index: dict[str, list[dict[str, Any]]]
) -> str:
    """'' | 'matched' (a byte-identical sibling is matched) | 'family' (≥3 unmatched clones)."""
    if body is None or body.n_instr < 8:  # tiny stubs fingerprint-collide meaninglessly
        return ""
    group = fp_index.get(body.fingerprint, [])
    if any(g["status"] == "matched" and g is not rec for g in group):
        return "matched"
    if len(group) >= 3:
        return "family"
    return ""


def run(top: int = 15) -> None:
    repo = repo_root()
    overrides = parse_overrides(repo / "config/triage_overrides.yaml")
    records, bodies = collect(repo)

    # Fingerprint index across every function that has an asm body.
    fp_index: dict[str, list[dict[str, Any]]] = {}
    for r in records:
        b = bodies.get(r["name"])
        if b is not None and b.n_instr:
            fp_index.setdefault(b.fingerprint, []).append(r)

    candidates: list[dict[str, Any]] = []
    skipped: list[dict[str, Any]] = []
    harvest: list[dict[str, Any]] = []
    for r in records:
        if r["lib"] or r.get("unsplit"):
            continue
        ov = overrides.get(r["name"], {})
        if r["status"] == "matched":
            continue
        if r["status"] == "wip" or ov.get("verdict") == "parked":
            r["note"] = ov.get("class", "") or ov.get("note", "")
            harvest.append(r)
            continue
        body = bodies.get(r["name"])
        classes = _blockers(r, body)
        if ov.get("verdict") == "wall":
            classes.append(ov.get("class", "wall (ledger)"))
        if classes:
            r["skip_classes"] = sorted(set(classes))
            skipped.append(r)
            continue
        clone = clone_class(r, body, fp_index)
        r["score"], r["why"] = _score(r, body, ov, clone)
        candidates.append(r)

    candidates.sort(key=lambda r: (-r["score"], int(r["addr"], 16)))
    harvest.sort(key=lambda r: (-r["partial_bytes"], int(r["addr"], 16)))
    skipped.sort(key=lambda r: (r["skip_classes"][0], int(r["addr"], 16)))

    out = render_md(candidates, harvest, skipped, top)
    (repo / "TRIAGE.md").write_text(out)

    print(f"triage: {len(candidates)} viable, {len(harvest)} parked/wip, {len(skipped)} walls")
    for r in candidates[:top]:
        print(
            f"  {r['score']:4d}  {r['addr']}  {r['name']}  ({r['segment']}, {r['size']}b)  "
            f"{'; '.join(r['why'])}"
        )
    print("  wrote TRIAGE.md")


def render_md(
    candidates: list[dict[str, Any]],
    harvest: list[dict[str, Any]],
    skipped: list[dict[str, Any]],
    top: int,
) -> str:
    lines: list[str] = []
    lines.append("# Triage — ranked match candidates")
    lines.append("")
    lines.append(
        "Generated by `make triage` (`open-spyro triage`). Session verdicts live in "
        "`config/triage_overrides.yaml` — append one entry per triaged function so no "
        "candidate is ever re-inspected from scratch. Classes reference the cookbook "
        "skip list / tie classes."
    )
    lines.append("")
    lines.append(f"## Top {top} candidates (vein / clone modes)")
    lines.append("")
    lines.append("| # | Address | Function | Segment | Size | Score | Why |")
    lines.append("|--:|---|---|---|--:|--:|---|")
    for i, r in enumerate(candidates[:top], 1):
        lines.append(
            f"| {i} | `{r['addr']}` | {r['name']} | {r['segment']} | {r['size']} | "
            f"{r['score']} | {'; '.join(r['why']) or '—'} |"
        )
    lines.append("")
    lines.append(f"({len(candidates)} viable candidates total; the full ranking is regenerable.)")
    lines.append("")
    lines.append("## Harvest inventory (parked / wip — permuter targets)")
    lines.append("")
    lines.append("| Address | Function | Segment | Size | Partial bytes | Class / note |")
    lines.append("|---|---|---|--:|--:|---|")
    for r in harvest:
        lines.append(
            f"| `{r['addr']}` | {r['name']} | {r['segment']} | {r['size']} | "
            f"{r['partial_bytes']} | {r.get('note', '')} |"
        )
    lines.append("")
    lines.append("## Auto-skipped (walls)")
    lines.append("")
    counts: dict[str, int] = {}
    for r in skipped:
        for c in r["skip_classes"]:
            counts[c] = counts.get(c, 0) + 1
    for c in sorted(counts):
        lines.append(f"- {c}: {counts[c]}")
    lines.append("")
    lines.append("| Address | Function | Segment | Size | Classes |")
    lines.append("|---|---|---|--:|---|")
    for r in skipped:
        lines.append(
            f"| `{r['addr']}` | {r['name']} | {r['segment']} | {r['size']} | "
            f"{', '.join(r['skip_classes'])} |"
        )
    lines.append("")
    return "\n".join(lines)
