"""The hand-asm translator's native patch hook (handasm_patches.py)."""

from __future__ import annotations

import pytest

from open_spyro.handasm import Cluster, Ctx, Func, assign_patches, decode, emit_cluster
from open_spyro.handasm_patches import Patch

BASE = 0x80000100


def addiu(rt: int, rs: int, imm: int) -> int:
    return (9 << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)


def sll(rd: int, rt: int, sa: int) -> int:
    return (rt << 16) | (rd << 11) | (sa << 6)


def bgez(rs: int, off: int) -> int:
    return (1 << 26) | (rs << 21) | (1 << 16) | (off & 0xFFFF)


T0, T1, T2, T3, RA = 8, 9, 10, 11, 31
WORDS = [
    addiu(T0, 0, 5),  # 100
    sll(T1, T0, 2),  # 104
    bgez(T1, 3),  # 108 -> 118
    addiu(T2, 0, 1),  # 10C (delay)
    addiu(T2, 0, 2),  # 110
    addiu(T3, 0, 3),  # 114
    (RA << 21) | 8,  # 118 jr ra
    0,  # 11C (delay)
]


def cluster(*patches: Patch) -> Cluster:
    f = Func("Test", BASE, list(WORDS))
    cl = Cluster("Test", [f])
    for k, w in enumerate(WORDS):
        cl.insns[BASE + 4 * k] = decode(BASE + 4 * k, w)
    assign_patches([cl], list(patches))
    return cl


def patch(
    start: int, end: int, c: str = "t1 = t0;", expect: tuple[int, ...] | None = None
) -> Patch:
    if expect is None:
        expect = tuple(WORDS[(start - BASE) // 4 : (end - BASE) // 4])
    return Patch("Test", start, end, expect, c, "test")


def emit(*patches: Patch) -> str:
    return emit_cluster(Ctx({}, {}, {}), cluster(*patches))


def test_unpatched_is_literal() -> None:
    text = emit()
    assert "bc = (" in text
    assert "replaced" not in text


def test_range_replaced_with_delay_slot() -> None:
    frag = "t1 = t0 << 3;\nt2 = 1u;\nif ((int32_t)t1 >= 0)\n  goto L_80000118;"
    text = emit(patch(BASE + 4, BASE + 0x10, frag))
    assert text.count("(replaced)") == 3
    assert "  t1 = t0 << 3;" in text
    assert "goto L_80000118;" in text
    assert "L_80000118:" in text
    assert "bc = (" not in text  # the only branch was replaced
    assert "/* 80000110 240A0002 */" in text  # control goes on at end


def test_range_must_take_the_delay_slot() -> None:
    with pytest.raises(SystemExit, match="ends between 80000108 and its delay slot"):
        emit(patch(BASE + 4, BASE + 0xC))


def test_range_may_not_start_in_a_delay_slot() -> None:
    with pytest.raises(SystemExit, match="starts in the delay slot of 80000108"):
        emit(patch(BASE + 0xC, BASE + 0x10))


def test_range_may_not_hold_a_branch_target() -> None:
    with pytest.raises(SystemExit, match="80000118 inside it is a jump target"):
        emit(patch(BASE + 0x10, BASE + 0x20))


def test_branch_target_may_start_a_range() -> None:
    text = emit(patch(BASE + 0x18, BASE + 0x20, "goto dispatch;"))
    assert "L_80000118:\n  /* native patch" in text


def test_stale_patch() -> None:
    with pytest.raises(SystemExit, match="stale: the asm there is now"):
        emit(patch(BASE + 0x10, BASE + 0x14, expect=(0x12345678,)))


def test_goto_must_name_a_label() -> None:
    with pytest.raises(SystemExit, match="not a label of Test"):
        emit(patch(BASE + 0x10, BASE + 0x14, "goto L_80000114;"))


def test_overlapping_patches() -> None:
    with pytest.raises(SystemExit, match="overlaps another patch"):
        emit(patch(BASE + 0x10, BASE + 0x18), patch(BASE + 0x14, BASE + 0x18))


def test_patch_outside_every_cluster() -> None:
    with pytest.raises(SystemExit, match="not in any hand-asm cluster"):
        cluster(patch(BASE + 0x100, BASE + 0x104, expect=(0,)))
