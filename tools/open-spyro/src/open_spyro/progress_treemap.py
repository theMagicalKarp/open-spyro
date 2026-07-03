"""The C-match "map of squares" — a squarified treemap of every function.

Reads the machine-readable ``progress.json`` (produced by ``make progress``) and
renders one SVG where each function is a rectangle sized by its code-byte count
and colored by match status:

    matched      green   — rebuilt bytes verified identical to the original
    asm          red     — still linked in as original assembly (the work left)
    handwritten  slate   — hand-authored asm; not C-matchable by design
    library      purple  — PSY-Q / libc leaf; excluded from the match denominator
    unsplit      amber   — un-split overlay blob (mostly level data); excluded until split
    blob         blue    — tagged level mega-blob (blob:True); split but excluded from the %

The layout is a classic squarified treemap (Bruls/Huizing/van Wijk): tiles keep a
near-1 aspect ratio so the eye reads relative size honestly. Pure stdlib — no
toolchain, no external service. Run via ``open-spyro progress-map``.
"""

from __future__ import annotations

import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any

# Layout constants (pixels).
WIDTH = 1200
PAD_TOP = 96  # header band
PAD = 12
LEGEND_H = 0

# GitHub-ish palette, matched to the legend categories.
COLORS = {
    "matched": "#3fb950",
    "asm": "#da3633",
    "handwritten": "#8b949e",
    "library": "#8957e5",
    "unsplit": "#9e6a03",
    "blob": "#388bfd",
    # "blob": "#8957e5",
}
BG = "#0d1117"
FG = "#e6edf3"
STROKE = "#0d1117"


def _category(f: dict[str, Any]) -> str:
    """Collapse a function record to one of the legend buckets."""
    if f["status"] == "matched":
        return "matched"
    if f.get("unsplit"):
        return "unsplit"
    if f.get("blob"):
        return "blob"
    if f.get("handwritten"):
        return "handwritten"
    if f.get("lib"):
        return "library"
    return "asm"


# ---- squarified treemap ------------------------------------------------------


def _worst(row: list[float], length: float, total_scale: float) -> float:
    """Worst (max) aspect ratio of a row laid along a side of given length."""
    s = sum(row) * total_scale
    side = length
    rmax, rmin = max(row) * total_scale, min(row) * total_scale
    return max(side * side * rmax / (s * s), s * s / (side * side * rmin))


def _squarify(
    values: list[float], x: float, y: float, w: float, h: float, scale: float
) -> list[tuple[float, float, float, float]]:
    """Return (x, y, w, h) rects for ``values`` filling the (x,y,w,h) box.

    ``values`` must be sorted descending and already scaled so sum == w*h/scale
    is irrelevant; ``scale`` converts value-units to pixel-area.
    """
    rects: list[tuple[float, float, float, float]] = []
    vals = list(values)
    while vals:
        length = min(w, h)
        row: list[float] = [vals[0]]
        while len(row) < len(vals):
            if _worst(row + [vals[len(row)]], length, scale) > _worst(row, length, scale):
                break
            row.append(vals[len(row)])
        # Lay this row along the shorter side.
        row_area = sum(row) * scale
        if w >= h:
            row_w = row_area / h if h else 0
            oy = y
            for v in row:
                rh = (v * scale) / row_w if row_w else 0
                rects.append((x, oy, row_w, rh))
                oy += rh
            x += row_w
            w -= row_w
        else:
            row_h = row_area / w if w else 0
            ox = x
            for v in row:
                rw = (v * scale) / row_h if row_h else 0
                rects.append((ox, y, rw, row_h))
                ox += rw
            y += row_h
            h -= row_h
        vals = vals[len(row) :]
    return rects


# ---- rendering ---------------------------------------------------------------


def _esc(s: str) -> str:
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def _short_seg(name: str) -> str:
    """Compact a segment name for the group label."""
    if name == "main":
        return "main (EXE)"
    n = name.replace("ovl/", "")
    return re.sub(r"^level_(\d+)_", r"\1 · ", n)


def _draw_tiles(
    out: list[str],
    funcs: list[dict[str, Any]],
    box: tuple[float, float, float, float],
    *,
    label_min: float = 60.0,
) -> None:
    """Squarify ``funcs`` into ``box`` and append their colored rects."""
    x0, y0, w0, h0 = box
    if w0 <= 0 or h0 <= 0:
        return
    fs = sorted(funcs, key=lambda f: f["size"], reverse=True)
    tot = sum(f["size"] for f in fs)
    if tot <= 0:
        return
    scale = (w0 * h0) / tot
    rects = _squarify([f["size"] for f in fs], x0, y0, w0, h0, scale)
    for f, (x, y, w, h) in zip(fs, rects, strict=True):
        cat = _category(f)
        out.append(
            f'<rect x="{x:.2f}" y="{y:.2f}" width="{max(w, 0):.2f}" '
            f'height="{max(h, 0):.2f}" fill="{COLORS[cat]}" stroke="{STROKE}" '
            f'stroke-width="0.5">'
            f"<title>{_esc(f['name'])}  ·  {f['size']} bytes  ·  {cat}</title>"
            f"</rect>"
        )
        if w > label_min and h > 16:
            label = f["name"]
            maxchars = int(w / 7.2)
            if len(label) > maxchars:
                label = label[: max(0, maxchars - 1)] + "…"
            tcol = "#0d1117" if cat in ("matched", "library") else FG
            out.append(
                f'<text x="{x + 4:.2f}" y="{y + 14:.2f}" fill="{tcol}" '
                f'font-size="11">{_esc(label)}</text>'
            )


def render_svg(data: dict[str, Any], grouped: bool = True) -> str:
    funcs = [f for f in data["functions"] if f["size"] > 0]

    pct = data["matched_pct"]  # already in percent units in progress.json
    mb = data["matched_bytes"]
    tb = data["total_code_bytes"]  # split game code only (denominator)
    unsplit_b = data.get("unsplit_overlay_bytes", 0)
    blob_b = data.get("blob_code_bytes", 0)
    n_match = sum(1 for f in funcs if f["status"] == "matched")

    inner_w = WIDTH - 2 * PAD
    inner_h = 1100 if grouped else 900
    height = PAD_TOP + inner_h + PAD

    out: list[str] = []
    out.append(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" '
        f'height="{height:.0f}" viewBox="0 0 {WIDTH} {height:.0f}" '
        f'font-family="ui-monospace,Menlo,Consolas,monospace">'
    )
    out.append(f'<rect width="{WIDTH}" height="{height:.0f}" fill="{BG}"/>')

    # Header.
    out.append(
        f'<text x="{PAD}" y="38" fill="{FG}" font-size="26" '
        f'font-weight="700">open-spyro — C-match progress</text>'
    )
    out.append(
        f'<text x="{PAD}" y="66" fill="{COLORS["matched"]}" font-size="22" '
        f'font-weight="700">{pct:.2f}% matched</text>'
    )
    out.append(
        f'<text x="{PAD + 190}" y="66" fill="#8b949e" font-size="16">'
        f"{mb:,} / {tb:,} split-code bytes · {n_match} matched · "
        f"{unsplit_b + blob_b:,} raw bytes (excluded)</text>"
    )
    # Legend.
    lx = WIDTH - PAD
    for label in ("blob", "unsplit", "library", "handwritten", "asm", "matched"):
        text = label
        tw = 9 * len(text) + 26
        lx -= tw
        out.append(f'<rect x="{lx}" y="20" width="14" height="14" rx="2" fill="{COLORS[label]}"/>')
        out.append(f'<text x="{lx + 20}" y="32" fill="{FG}" font-size="14">{text}</text>')
        lx -= 12

    if not grouped:
        _draw_tiles(out, funcs, (PAD, PAD_TOP, inner_w, inner_h))
        out.append("</svg>")
        return "\n".join(out)

    # Grouped: outer treemap over segments ("layers"), inner treemap per segment.
    groups: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for f in funcs:
        groups[f["segment"]].append(f)
    seg_items = sorted(groups.items(), key=lambda kv: sum(x["size"] for x in kv[1]), reverse=True)
    seg_sizes = [sum(x["size"] for x in fs) for _, fs in seg_items]
    grand = sum(seg_sizes)
    scale = (inner_w * inner_h) / grand
    seg_rects = _squarify(seg_sizes, PAD, PAD_TOP, inner_w, inner_h, scale)

    LBL = 16  # per-group header strip height
    GAP = 2  # gutter between layers
    for (seg, fs), (x, y, w, h) in zip(seg_items, seg_rects, strict=True):
        gx, gy, gw, gh = x + GAP, y + GAP, w - 2 * GAP, h - 2 * GAP
        if gw <= 0 or gh <= 0:
            continue
        seg_bytes = sum(f["size"] for f in fs)
        seg_match = sum(f["size"] for f in fs if f["status"] == "matched")
        seg_pct = 100 * seg_match / seg_bytes if seg_bytes else 0
        # Group frame + header strip.
        out.append(
            f'<rect x="{gx:.2f}" y="{gy:.2f}" width="{gw:.2f}" height="{gh:.2f}" '
            f'fill="none" stroke="#30363d" stroke-width="1"/>'
        )
        strip = min(LBL, gh)
        out.append(
            f'<rect x="{gx:.2f}" y="{gy:.2f}" width="{gw:.2f}" height="{strip:.2f}" '
            f'fill="#161b22"/>'
        )
        if gw > 44:
            label = _short_seg(seg)
            maxchars = int((gw - 40) / 6.3)
            if len(label) > maxchars:
                label = label[: max(0, maxchars - 1)] + "…"
            out.append(
                f'<text x="{gx + 4:.2f}" y="{gy + 12:.2f}" fill="{FG}" '
                f'font-size="11" font-weight="600">{_esc(label)}</text>'
            )
            out.append(
                f'<text x="{gx + gw - 3:.2f}" y="{gy + 12:.2f}" fill="#8b949e" '
                f'font-size="10" text-anchor="end">{seg_pct:.0f}%</text>'
            )
        _draw_tiles(out, fs, (gx, gy + strip, gw, gh - strip), label_min=1e9)

    out.append("</svg>")
    return "\n".join(out)


def run(progress_json: Path | None = None, out_path: Path | None = None) -> Path:
    repo = Path(__file__).resolve().parents[4]
    progress_json = progress_json or repo / "progress.json"
    out_path = out_path or repo / "docs" / "progress_map.svg"
    data = json.loads(progress_json.read_text())
    out_path.write_text(render_svg(data))
    return out_path


if __name__ == "__main__":
    print(run())
