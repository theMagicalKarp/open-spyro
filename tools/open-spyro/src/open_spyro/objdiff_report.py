"""An objdiff-format progress report (``build/report.json``) for progress boards.

objdiff's report (objdiff-core/protos/report.proto, v2) is the format decomp.dev
and recomp.fyi read. We don't run objdiff — the build is one big asm/text.s with
per-function HAVE_C slots, not per-unit target/base objects — so this module
re-expresses ``progress``'s verified records in that schema, with one unit per
segment (main EXE + each overlay). Serialized the way objdiff's pbjson does it:
proto field names, 64-bit integers as strings.

--- What the numbers mean ----------------------------------------------------
code   The same game-code accounting as PROGRESS.md: a function is matched iff it
       is defined in C and its rebuilt bytes equal the original. Stock PSY-Q/libc
       and Insomniac's hand-written asm stay out of the denominator, exactly as
       PROGRESS.md documents.
data   Measured the way objdiff would: only bytes a compiled C object emits count
       as matched. That is the jump tables / const pieces a matched C function
       owns (config/**/text_layout.json `rodata` pieces), placed into their
       original slots. Everything else in .rodata/.data is still assembled from
       asm/, so it is unmatched even though the linked image is byte-identical.
       The data denominator is the whole linked .rodata + .data of each segment
       (read from the ld map), SDK data included — it cannot be attributed.
complete
       Always 0: no unit is linked purely from C (every segment still links its
       asm/ objects for data), so none is "complete" in objdiff's sense.
"""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any

REPORT_VERSION = 2

_MAP_SIZE = re.compile(r"^\s*0x([0-9a-fA-F]+)\s+(\w+)_(RODATA|DATA)_SIZE\s*=")


def _map_and_layout(repo: Path, segment: str) -> tuple[Path, str, Path]:
    """(ld map, section-symbol prefix, text_layout.json) for a progress segment."""
    if segment == "main":
        return repo / "build/main/spyro_main.map", "main", repo / "config/text_layout.json"
    name = segment.removeprefix("ovl/")
    return (
        repo / "build/overlays" / f"{name}.map",
        name,
        repo / "config/overlays" / f"{name}.text_layout.json",
    )


def _data_size(map_path: Path, prefix: str) -> int:
    """Linked .rodata + .data bytes of a segment, from its ld map."""
    if not map_path.exists():
        raise FileNotFoundError(f"{map_path} missing — build before writing the report")
    total = 0
    for line in map_path.read_text().splitlines():
        m = _MAP_SIZE.match(line)
        if m and m.group(2) == prefix:
            total += int(m.group(1), 16)
    return total


def _c_rodata(layout: Path, matched: set[str]) -> int:
    """Bytes of rodata pieces owned by matched C functions (emitted by their .o)."""
    if not layout.exists():
        return 0
    pieces = json.loads(layout.read_text()).get("rodata", [])
    return sum(p["size"] for p in pieces if p.get("owner") in matched)


def _pct(part: int, whole: int) -> float:
    return part / whole * 100 if whole else 100.0


Counts = tuple[int, int]  # (matched, total)


def _measures(code: Counts, data: Counts, funcs: Counts, units: int) -> dict[str, Any]:
    (matched_code, total_code), (matched_data, total_data) = code, data
    matched_functions, total_functions = funcs
    code_pct = _pct(matched_code, total_code)
    return {
        "fuzzy_match_percent": code_pct,
        "total_code": str(total_code),
        "matched_code": str(matched_code),
        "matched_code_percent": code_pct,
        "total_data": str(total_data),
        "matched_data": str(matched_data),
        "matched_data_percent": _pct(matched_data, total_data),
        "total_functions": total_functions,
        "matched_functions": matched_functions,
        "matched_functions_percent": _pct(matched_functions, total_functions),
        "complete_code": "0",
        "complete_code_percent": 0.0,
        "complete_data": "0",
        "complete_data_percent": 0.0,
        "total_units": units,
        "complete_units": 0,
    }


def _rollup(rows: list[tuple[Counts, Counts, Counts]]) -> dict[str, Any]:
    """Measures summed over several units' (code, data, funcs) counts."""

    def total(i: int) -> Counts:
        return sum(r[i][0] for r in rows), sum(r[i][1] for r in rows)

    return _measures(total(0), total(1), total(2), len(rows))


def build(repo: Path, records: list[dict[str, Any]]) -> dict[str, Any]:
    """The report for ``progress`` records (already verified against the build)."""
    by_seg: dict[str, list[dict[str, Any]]] = {}
    for r in records:
        by_seg.setdefault(r["segment"], []).append(r)

    units = []
    counts: dict[str, list[tuple[Counts, Counts, Counts]]] = {"main": [], "overlays": []}
    for segment in sorted(by_seg, key=lambda s: (s != "main", s)):
        recs = by_seg[segment]
        game = [r for r in recs if not r["lib"] and not r["handwritten"] and not r["unsplit"]]
        matched_names = {r["name"] for r in recs if r["status"] == "matched"}
        map_path, prefix, layout = _map_and_layout(repo, segment)

        code = (sum(r["matched_bytes"] for r in game), sum(r["size"] for r in game))
        data = (_c_rodata(layout, matched_names), _data_size(map_path, prefix))
        funcs = (sum(r["status"] == "matched" for r in game), len(game))
        category = "main" if segment == "main" else "overlays"
        counts[category].append((code, data, funcs))

        units.append(
            {
                "name": f"{category}/{prefix}",
                "measures": _measures(code, data, funcs, 1),
                "sections": [],
                "functions": [
                    {
                        "name": r["name"],
                        "size": str(r["size"]),
                        "fuzzy_match_percent": 100.0 if r["status"] == "matched" else 0.0,
                        "metadata": {"virtual_address": str(int(r["addr"], 16))},
                    }
                    for r in game
                ],
                "metadata": {
                    "complete": False,
                    "module_name": prefix,
                    "progress_categories": [category],
                    "auto_generated": True,
                },
            }
        )

    return {
        "measures": _rollup(counts["main"] + counts["overlays"]),
        "units": units,
        "version": REPORT_VERSION,
        "categories": [
            {"id": "main", "name": "Main EXE", "measures": _rollup(counts["main"])},
            {"id": "overlays", "name": "Overlays", "measures": _rollup(counts["overlays"])},
        ],
    }


def write(repo: Path, records: list[dict[str, Any]], out: Path) -> dict[str, Any]:
    report = build(repo, records)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=2) + "\n")
    return report["measures"]
