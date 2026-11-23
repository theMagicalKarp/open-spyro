"""Native-only changes to the hand-asm translation (``handasm.py``).

The literal translation reproduces the original exactly, including its 4:3
screen-edge culls. A patch replaces the C emitted for an address range of
one cluster with a fragment of its own, for the native build's widescreen
view (``include/native_view.h``). Only ``src/native/handasm`` is generated
from these; the PS1 build never sees them.

Rules, checked by the generator (``apply_patches``):

- ``[start, end)`` lies inside one cluster and neither end splits a
  transfer from its delay slot: a range ending in a branch takes the delay
  slot too, and the fragment runs that instruction itself.
- No instruction inside the range other than ``start`` is a jump target
  (a label), and no load-delay hazard crosses either end.
- ``expect`` holds the original instruction words, so a re-split that moves
  the code fails instead of patching the wrong instructions.
- The fragment may ``goto L_XXXXXXXX`` to a label of the cluster that is not
  a delay slot; if it does not jump, control goes on at ``end``.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Patch:
    func: str  # the function the range is in (for the message and the comment)
    start: int
    end: int  # exclusive
    expect: tuple[int, ...]  # the original words of [start, end)
    c: str  # the C that replaces them (statements over the register locals)
    why: str
    includes: tuple[str, ...] = ("native_view.h",)


# RenderWorldChunks tests each chunk's bounding sphere (view-space centre
# x = a2, y = a3, z = t0 after the GTE rotation; radius terms t3 ~ 0.78r and
# t4 ~ 0.59r) against the side planes 4|x| = 3z, the 4:3 view's edges:
# outside when 4|x| - 3z >= 4t3 + 3t4 (about 5r, the plane normal's length
# times r), wholly inside when 4|x| - 3z < -(4t3 + 3t4). Widened, the plane
# is 4|x| = 3kz (k the view's half-width over 256: VIEW_WIDEN) and the radius
# term grows with its normal (VIEW_SPHERE); both are exact at 4:3.
_CHUNK_SIDE = "(int32_t)(4 * (int32_t)a2 - VIEW_WIDEN(3 * (int32_t)t0))"
_CHUNK_RADIUS = "VIEW_SPHERE(4 * (int32_t)t3 + 3 * (int32_t)t4)"


def _x_outcode(func: str, start: int, expect: tuple[int, int], sxy: str, out: str) -> Patch:
    """A ``bltz; andi OUT, SXY, 0xFE00`` pair (the andi in the delay slot):
    the x outcode of a projected vertex, nonzero when x is outside 0..511.
    Widened to the view; OUT keeps the original's value when nonzero (the
    callers only test it against zero)."""
    target = start + 4 + ((expect[0] & 0xFFFF) << 2)
    return Patch(
        func,
        start,
        start + 8,
        expect,
        f"""
bc = (int32_t){out} < 0;
{out} = VIEW_X_OUTSIDE({sxy}) ? {sxy} & 0xFE00u : 0u;
if (bc)
  goto L_{target:08X};
""",
        "a vertex's x outcode: outside the view, not 0..511",
    )


PATCHES: list[Patch] = [
    Patch(
        "RenderWorldChunks",
        0x80025AAC,
        0x80025AC8,
        (0x00CB0822, 0x00010880, 0x00021840, 0x00431020, 0x00220822, 0x0421FFCE, 0x000A68C2),
        f"""
v1 = v0 << 1;
v0 = v0 + v1;
at = (uint32_t)({_CHUNK_SIDE} - {_CHUNK_RADIUS});
t5 = t2 >> 3;
if ((int32_t)at >= 0)
  goto L_800259FC;
""",
        "chunk wholly past a side edge: skipped",
    ),
    Patch(
        "RenderWorldChunks",
        0x80025B0C,
        0x80025B2C,
        (
            0x00CB0820,
            0x010C1022,
            0x00010880,
            0x00021840,
            0x00431020,
            0x00220822,
            0x04210009,
            0x00ED0820,
        ),
        f"""
v0 = t0 - t4;
v1 = v0 << 1;
v0 = v0 + v1;
bc = {_CHUNK_SIDE} + {_CHUNK_RADIUS} >= 0;
at = a3 + t5;
if (bc)
  goto L_80025B4C;
""",
        "chunk wholly inside both side edges (with the vertical test after it)",
    ),
    _x_outcode("RenderWorldChunks", 0x80026330, (0x04A00002, 0x3045FE00), "v0", "a1"),
    _x_outcode("RenderWorldChunks", 0x8002698C, (0x04600002, 0x3083FE00), "a0", "v1"),
    _x_outcode("RenderWorldChunks", 0x80026B18, (0x04600002, 0x3083FE00), "a0", "v1"),
]
