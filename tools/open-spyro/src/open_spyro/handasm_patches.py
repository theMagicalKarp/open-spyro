"""Native-only changes to the hand-asm translation (``handasm.py``).

The literal translation reproduces the original exactly, including its 4:3
screen-edge culls. A patch replaces the C emitted for an address range of
one cluster with a fragment of its own, for the native build's widescreen
view (``include/native_view.h``). Only ``src/native/handasm`` is generated
from these; the PS1 build never sees them.

Rules, checked by the generator (``check_patches``):

- ``[start, end)`` lies inside one cluster and neither end splits a
  transfer from its delay slot: a range ending in a branch takes the delay
  slot too, and the fragment runs that instruction itself.
- No instruction inside the range other than ``start`` is a jump target
  (a label), and no load-delay hazard crosses either end.
- ``expect`` holds the original instruction words, so a re-split that moves
  the code fails instead of patching the wrong instructions.
- The fragment may ``goto L_XXXXXXXX`` to a label of the cluster that is not
  a delay slot; if it does not jump, control goes on at ``end``.

A ``BranchPatch`` is the narrow form: it replaces only the condition of one
conditional branch, so the branch, its delay slot and its target stay the
literal translation. The condition is evaluated before the delay slot runs,
as the branch's own is.
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


@dataclass(frozen=True)
class BranchPatch:
    func: str
    addr: int  # the conditional branch
    expect: int  # its original word
    cond: str  # the C condition that replaces its own
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


def _side_sphere(
    func: str,
    start: int,
    expect: tuple[int, ...],
    regs: tuple[str, str, str],
    radius: tuple[str, str],
    straddle: bool,
    delay: str,
) -> Patch:
    """An actor or sprite bounding sphere against the side planes 4|x| = 3z,
    in the form ``B = Z -+ RZ; A = A +- RX; A = 4A - B - 2B; bgez A`` with
    the branch's delay slot last. A holds |x| (view space), Z the depth and
    B is scratch; RX and RZ (registers or constants) are the radius terms,
    about 0.78r and 0.59r. The reject form (``straddle`` false) branches
    when the sphere is wholly past an edge, 4|x| - 3z >= 4RX + 3RZ; the
    straddle form when it reaches past one, 4|x| - 3z >= -(4RX + 3RZ).
    Widened like the chunk test: the depth term by VIEW_WIDEN, the radius
    term by VIEW_SPHERE, so it is exact at 4:3. B ends as the original's."""
    a, z, b = regs
    rx, rz = radius
    off = expect[6] & 0xFFFF
    target = start + 0x1C + ((off - 0x10000 if off & 0x8000 else off) << 2)
    sign, zop = ("+", "-") if straddle else ("-", "+")
    return Patch(
        func,
        start,
        start + 0x20,
        expect,
        f"""
{b} = {z} {zop} {rz};
{a} = (uint32_t)(4 * (int32_t){a} - VIEW_WIDEN(3 * (int32_t){z})
      {sign} VIEW_SPHERE(4 * (int32_t)({rx}) + 3 * (int32_t)({rz})));
{b} = {b} << 1;
bc = (int32_t){a} >= 0;
{delay}
if (bc)
  goto L_{target:08X};
""",
        "sphere reaches past a side edge: clipped path"
        if straddle
        else "sphere wholly past a side edge: skipped",
    )


# BuildActorDrawList and BuildSecondaryActorDrawList (same code): an
# actor's view-space centre (x = at, y = v0, z = v1, |x| in t2) and radius
# terms a2 / a3 of its model's bounding radius.
_ACTOR_REJECT = (
    0x00675820,
    0x01465022,
    0x000A5080,
    0x014B5022,
    0x000B5840,
    0x014B5022,
    0x0541FFB1,
    0x8FAB001C,
)
_ACTOR_STRADDLE = (
    0x00675822,
    0x01465020,
    0x000A5080,
    0x014B5022,
    0x000B5840,
    0x014B5022,
    0x0541000D,
    0x00000000,
)
_ACTOR_REGS = ("t2", "v1", "t3")
_ACTOR_RADIUS = ("a2", "a3")
_ACTOR_DELAY = "t3 = HA_LW(sp + 28u);"


_REG = (
    "zero at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 "
    "s0 s1 s2 s3 s4 s5 s6 s7 t8 t9 k0 k1 gp sp fp ra"
).split()


def _x_hi(func: str, addr: int, word: int) -> BranchPatch:
    """A screen-x test on a GTE x held in a register's high half: ``bgtz`` /
    ``blez`` on sxy << 16 (against the left edge, x > 0), or ``bltz`` /
    ``bgez`` on (sxy << 16) - (512 << 16) (the right edge, x < 512).
    Widened to the view's edges (VIEW_X0_HI / VIEW_X1_HI_REL); the branch's
    op and register come from its word."""
    op, rs, rt = word >> 26, _REG[(word >> 21) & 31], (word >> 16) & 31
    cond = {(7, 0): ">", (6, 0): "<=", (1, 0): "<", (1, 1): ">="}[(op, rt if op == 1 else 0)]
    edge = "VIEW_X0_HI" if op in (6, 7) else "VIEW_X1_HI_REL"
    side = "left" if op in (6, 7) else "right"
    return BranchPatch(
        func, addr, word, f"(int32_t){rs} {cond} {edge}", f"screen x against the view's {side} edge"
    )


# Each right test's bound is a register holding 512 << 16 (0x2000000) for
# the whole loop or function.
_X_HI = [
    # Per-vertex outcodes in the polygon emitters, left then right.
    ("EmitActorDrawList", 0x8001FC90, 0x1DC00002),
    ("EmitActorDrawList", 0x8001FC9C, 0x05C00002),
    ("EmitActorDrawList", 0x8001FDA0, 0x1DC00002),
    ("EmitActorDrawList", 0x8001FDAC, 0x05C00002),
    ("EmitSecondaryActorPrimitives", 0x80021450, 0x1DC00002),
    ("EmitSecondaryActorPrimitives", 0x8002145C, 0x05C00002),
    ("EmitSecondaryActorPrimitives", 0x80021578, 0x1DC00002),
    ("EmitSecondaryActorPrimitives", 0x80021584, 0x05C00002),
    ("EmitSkyMesh", 0x8004EE28, 0x1CA00002),
    ("EmitSkyMesh", 0x8004EE34, 0x04A00002),
    ("EmitSkyMeshFogged", 0x8004F2D8, 0x1CA00002),
    ("EmitSkyMeshFogged", 0x8004F2E4, 0x04A00002),
    ("RasterizeSpritePrimQueue", 0x800230A8, 0x1C800002),
    ("RasterizeSpritePrimQueue", 0x800230B4, 0x04800002),
    ("RenderWorldChunks", 0x80026AC0, 0x1C600002),
    ("RenderWorldChunks", 0x80026ACC, 0x04600002),
    ("RenderWorldChunks", 0x80027B04, 0x1C400002),
    ("RenderWorldChunks", 0x80027B10, 0x04400002),
    ("RenderWorldChunks", 0x80028220, 0x1C400002),
    ("RenderWorldChunks", 0x8002822C, 0x04400002),
    ("RenderWorldChunks", 0x80028CFC, 0x1C400002),
    ("RenderWorldChunks", 0x80028D08, 0x04400002),
    ("RenderWorldChunks", 0x80029A58, 0x1C400002),
    ("RenderWorldChunks", 0x80029A64, 0x04400002),
    ("RenderWorldChunks", 0x8002A31C, 0x1CE00002),
    ("RenderWorldChunks", 0x8002A328, 0x04E00002),
    ("RenderWorldChunks", 0x8002A4E8, 0x1CE00002),
    ("RenderWorldChunks", 0x8002A4F4, 0x04E00002),
    # RasterizeEmitList: a quad (any corner inside each edge, else skipped),
    # then the single-point records (the point itself).
    ("RasterizeEmitList", 0x800576D0, 0x1CA00006),
    ("RasterizeEmitList", 0x800576D8, 0x1CA00004),
    ("RasterizeEmitList", 0x800576E0, 0x1CA00002),
    ("RasterizeEmitList", 0x800576E8, 0x18A0FF75),
    ("RasterizeEmitList", 0x800576F4, 0x04A00009),
    ("RasterizeEmitList", 0x80057700, 0x04A00006),
    ("RasterizeEmitList", 0x8005770C, 0x04A00003),
    ("RasterizeEmitList", 0x80057718, 0x04A1FF69),
    ("RasterizeEmitList", 0x80057A00, 0x1860FEAF),
    ("RasterizeEmitList", 0x80057A08, 0x0461FEAD),
    ("RasterizeEmitList", 0x80057B10, 0x1880FE6B),
    ("RasterizeEmitList", 0x80057B18, 0x0481FE69),
    ("RasterizeEmitList", 0x80057CB0, 0x18E0FE03),
    ("RasterizeEmitList", 0x80057CB8, 0x04E1FE01),
    ("RasterizeEmitList", 0x80057F24, 0x1860FD66),
    ("RasterizeEmitList", 0x80057F2C, 0x0461FD64),
    ("RasterizeEmitList", 0x80058018, 0x1880FD29),
    ("RasterizeEmitList", 0x80058020, 0x0481FD27),
    # DrawWorldRotatingBillboards: the billboard's centre.
    ("DrawWorldRotatingBillboards", 0x8005864C, 0x18A00071),
    ("DrawWorldRotatingBillboards", 0x80058658, 0x04A1006E),
]


def _x_lo(func: str, addr: int, word: int) -> BranchPatch:
    """An outcode bit on a plain screen x: ``bgtz X`` (left edge, x > 0) or
    ``bltz T`` on T = X - 512 (right edge, x < 512). Widened to the culls'
    edges; exact at 4:3."""
    op, rs = word >> 26, _REG[(word >> 21) & 31]
    if op == 7:
        return BranchPatch(
            func, addr, word, f"(int32_t){rs} > CULL_X0", "screen x against the view's left edge"
        )
    return BranchPatch(
        func,
        addr,
        word,
        f"(int32_t){rs} < CULL_X1 - SCREEN_W",
        "screen x against the view's right edge",
    )


# DrawWorldSparkleParticles: the outcodes of a sparkle's centre, then of
# each of its two streak ends (all three share a bit to be skipped).
_X_LO = [
    ("DrawWorldSparkleParticles", 0x800582E0, 0x05200002),
    ("DrawWorldSparkleParticles", 0x800582EC, 0x1CA00002),
    ("DrawWorldSparkleParticles", 0x80058398, 0x05A00002),
    ("DrawWorldSparkleParticles", 0x800583A4, 0x1D400002),
    ("DrawWorldSparkleParticles", 0x80058418, 0x05A00002),
    ("DrawWorldSparkleParticles", 0x80058424, 0x1D400002),
]


# RasterizeSpyroModel skips Spyro's model when his view-space centre is past
# the planes |x| = z + 0x800 (a0 = z + 0x800, x in at): 45 degrees, wider
# than the 4:3 view's 37 but narrower than 16:9's 45.3 and beyond. Widened
# with the view like the other side culls (VIEW_WIDEN, exact at 4:3); its
# vertical pair stays.
_SPYRO_SIDE = [
    BranchPatch(
        "RasterizeSpyroModel",
        0x80023F6C,
        0x18A00650,
        "VIEW_WIDEN((int32_t)a0) - (int32_t)at <= 0",
        "Spyro past the right side plane: not drawn",
    ),
    BranchPatch(
        "RasterizeSpyroModel",
        0x80023F74,
        0x18A0064E,
        "VIEW_WIDEN((int32_t)a0) + (int32_t)at <= 0",
        "Spyro past the left side plane: not drawn",
    ),
]


def _sky_near(func: str, addr: int, word: int) -> BranchPatch:
    """A sky segment skipped when its centre is within its radius of the
    camera plane (``blez v0`` on v0 = z - r, r in at): the sky has no near
    clipping, so those would project vertices from behind the camera, and at
    4:3 they were never on screen. A wider view sees past the old edges into
    segments beside the camera, so the margin shrinks with it (VIEW_NARROW,
    exact at 4:3)."""
    return BranchPatch(
        func,
        addr,
        word,
        "(int32_t)v0 + (int32_t)at <= VIEW_NARROW((int32_t)at)",
        "sky segment too near the camera plane",
    )


# The world sky, its fogged copy, and the portal skies (two segment walks).
_SKY_NEAR = [
    ("EmitSkyMesh", 0x8004ECC4, 0x1840FFE4),
    ("EmitSkyMeshFogged", 0x8004F0E0, 0x1840FFEE),
    ("EmitPortalSky", 0x8004F588, 0x1840FFEE),
    ("EmitPortalSky", 0x8005030C, 0x1840FFEE),
]


PATCHES: list[Patch | BranchPatch] = [
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
    _side_sphere(
        "BuildActorDrawList",
        0x8001F308,
        _ACTOR_REJECT,
        _ACTOR_REGS,
        _ACTOR_RADIUS,
        False,
        _ACTOR_DELAY,
    ),
    _side_sphere(
        "BuildActorDrawList", 0x8001F3B0, _ACTOR_STRADDLE, _ACTOR_REGS, _ACTOR_RADIUS, True, ""
    ),
    _side_sphere(
        "BuildSecondaryActorDrawList",
        0x80020AB4,
        _ACTOR_REJECT,
        _ACTOR_REGS,
        _ACTOR_RADIUS,
        False,
        _ACTOR_DELAY,
    ),
    _side_sphere(
        "BuildSecondaryActorDrawList",
        0x80020B5C,
        _ACTOR_STRADDLE,
        _ACTOR_REGS,
        _ACTOR_RADIUS,
        True,
        "",
    ),
    # RasterizeSpritePrimQueue: a sprite as a sphere of radius 0x80 (terms
    # 0x66 and 0x4D), centre x = at, z = v1, |x| in a1.
    _side_sphere(
        "RasterizeSpritePrimQueue",
        0x80022C08,
        (
            0x2066004D,
            0x20A5FF9A,
            0x00052880,
            0x00A62822,
            0x00063040,
            0x00A62822,
            0x04A1FFB9,
            0x02D5B020,
        ),
        ("a1", "v1", "a2"),
        ("0x66", "0x4D"),
        False,
        "s6 = s6 + s5;",
    ),
    _side_sphere(
        "RasterizeSpritePrimQueue",
        0x80022C80,
        (
            0x2066FFB3,
            0x20A50066,
            0x00052880,
            0x00A62822,
            0x00063040,
            0x00A62822,
            0x04A1000C,
            0x93C40057,
        ),
        ("a1", "v1", "a2"),
        ("0x66", "0x4D"),
        True,
        "a0 = HA_LBU(fp + 87u);",
    ),
    *(_x_hi(*args) for args in _X_HI),
    *(_sky_near(*args) for args in _SKY_NEAR),
    *(_x_lo(*args) for args in _X_LO),
    *_SPYRO_SIDE,
]
