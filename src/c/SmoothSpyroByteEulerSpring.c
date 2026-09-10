#include "globals.h"

extern int g_nSpyroSwingEulerPitchTarget;
extern int g_nCameraScriptStickRequestY;
extern int g_nCameraScriptStickRequestX;
extern int g_nSpyroSwingEulerPrev0;
extern int g_nSpyroSwingEulerPrev1;
extern int g_nSpyroSwingEulerPrev2;
extern int g_nSpyroSwingEulerSpringAccum0;
extern int g_nSpyroSwingEulerSpringAccum1;
extern int g_nSpyroSwingEulerSpringAccum2;

/* 0x80049880: critically-damped spring that carries Spyro's three swing euler
   angles toward their targets and publishes them as byte angles. Per axis: the
   12-bit signed error drives a velocity accumulator (error*128 minus the
   current accumulator*16, scaled by 1/64), the accumulator advances the
   angle by 1/64, and the angle is republished as a byte euler (>> 4). */
/* MATCHED 2026-09-10. The block structure, read off the branch targets: each
   stanza is `delta; guard; spring`, but the NEXT stanza's delta is computed
   BEFORE the current stanza's spring (the original software-pipelines it in
   source: the delta subu/andi sit in the join block of the previous guard), and
   the byte-0 and byte-1 publications sit between the THIRD guard and the third
   spring. Writing three uniform self-contained stanzas costs ~60 insns.

   Five load-bearing levers, all measured — do not "clean up" any of them:
     - A186 REFINEMENT: the first two guards show `sll f,d,7` in the delay slot
       AND again after the arm. Writing that join insn ONCE is only reachable
       when its dest is not the guarded variable's register: written once it is
       block-local, local-alloc ties its dest to the dying `d`, and reorg cannot
       copy it into the slot at all. `f = d << 7;` BEFORE the guard AND inside
       the arm makes `f` cross-block and both slots come out with the original's
       duplicate. The third guard must NOT use that form.
     - the accumulator is a PER-STANZA local (A138/A146); one shared variable is
       a cross-block allocno, conflicts with v0 and lands in a0.
     - `d` is ONE shared variable across all three stanzas.
     - the >>6 is its own statement (A167): `acc = acc + (f >> 6)` makes a fresh
       temp instead of shifting in place.
     - stanza 2 computes into its OWN block-scoped `f2`, not the shared `f` —
       the original's stanza-2 chain sits in v1 with the `acc2 << 4` temp in
       `f`'s a0, i.e. `f` is not live there at all.

   THE LAST FOUR INSNS WERE THREE A195/A200 `do { } while (0)` DIALS; the
   mechanism matters if you ever meet this shape again (every number below is
   from `cc1 -dl`/`-dg`, the §D 2c probe):
     1. EMPTY barrier after the byte-1 store. Without it sched1 hoists the
        accum-2 load and the `d << 7` above that store (21 insns of diff, and
        every positional residue in the function). With it the whole instruction
        sequence is position-for-position identical. 21 -> 13.
     2. The prev1 and prev2 held loads swap a3 <-> t0 because the two pseudos
        have the same 3 refs and live lengths that differ by ONE insn (29 vs
        28), so global.c's `floor_log2(R)*R/L` allocates prev2 first (3/28 =
        .1071 > 3/29 = .1034). Statement order CANNOT fix this — the lengths are
        post-sched1 (F14b), and a six-position sweep of the stanza-1 delta
        leaves them unchanged. The dial is A195: flow.c weights every ref by
        `loop_depth`, so a do/while around a statement adds one ref per
        reference in it, and 3 -> 4 refs crosses a `floor_log2` step (priority
        .1034 -> .276). Wrapping the prev1 UPDATE alone overshoots past prev0
        too (a2/a3 rotate, 18); wrapping the prev0 DELTA as well restores the
        chain prev0 > prev1 > prev2. 13 -> 8 -> 3.
     3. `f` loses its stanza-2 references to `f2`, which drops it below `d` and
        rotates a0/a1 through the whole function (22). An A195 wrap on stanza
        0's `f = f >> 6; acc0 = acc0 + f;` pair puts it back. -> 3.
     4. The last 3 insns were the A200 barrier's OWN cost: the original issues
        the stanza-2 guard's `slti` BEFORE the prev1 store, and the dial's
        closing NOTE_INSN_LOOP_END sits between them, where nothing can cross it
        (A200c). The fix is to put the guard `if` INSIDE the same wrapper as the
        prev1 update, so the compare is above the closing note. -> MATCH.
     GENERAL RULE worth keeping: when an A195 ref dial's barrier cost is "one
     insn of the FOLLOWING block wants to be above my store", widen the wrapper
     to swallow that insn rather than moving the dial. */
void SmoothSpyroByteEulerSpring(void) {
  int d;
  int f;
  int acc0;
  int acc1;
  int acc2;

  do {
    d = (g_nSpyroSwingEulerPitchTarget - g_nSpyroSwingEulerPrev0) & 0xFFF;
  } while (0);
  f = d << 7;
  if (d > 0x800) {
    d = d - 0x1000;
    f = d << 7;
  }

  d = (g_nCameraScriptStickRequestY - g_nSpyroSwingEulerPrev1) & 0xFFF;
  acc0 = g_nSpyroSwingEulerSpringAccum0;
  f = f - (acc0 << 4);
  do {
    f = f >> 6;
    acc0 = acc0 + f;
  } while (0);
  g_nSpyroSwingEulerSpringAccum0 = acc0;
  g_nSpyroSwingEulerPrev0 = g_nSpyroSwingEulerPrev0 + (acc0 >> 6);

  f = d << 7;
  if (d > 0x800) {
    d = d - 0x1000;
    f = d << 7;
  }

  d = (g_nCameraScriptStickRequestX - g_nSpyroSwingEulerPrev2) & 0xFFF;
  acc1 = g_nSpyroSwingEulerSpringAccum1;
  f = f - (acc1 << 4);
  f = f >> 6;
  acc1 = acc1 + f;
  g_nSpyroSwingEulerSpringAccum1 = acc1;

  do {
    g_nSpyroSwingEulerPrev1 = g_nSpyroSwingEulerPrev1 + (acc1 >> 6);
    if (d > 0x800) {
      d = d - 0x1000;
    }
  } while (0);
  g_abSpyroSwingByteEuler[0] = g_nSpyroSwingEulerPrev0 >> 4;
  g_abSpyroSwingByteEuler[1] = g_nSpyroSwingEulerPrev1 >> 4;
  do {
  } while (0);
  acc2 = g_nSpyroSwingEulerSpringAccum2;
  {
    int f2;
    f2 = (d << 7) - (acc2 << 4);
    f2 = f2 >> 6;
    acc2 = acc2 + f2;
  }
  g_nSpyroSwingEulerSpringAccum2 = acc2;
  g_nSpyroSwingEulerPrev2 = g_nSpyroSwingEulerPrev2 + (acc2 >> 6);
  g_abSpyroSwingByteEuler[2] = g_nSpyroSwingEulerPrev2 >> 4;
}
