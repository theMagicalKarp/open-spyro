#include "actor.h"
#include "actor_type.h"
#include "cd_stream.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"

/* Update for gamestate 0xC (level-transition title card, 0x800324d8, 0x548).

   Substate 4 is the only one this function drives; every other substate is
   forwarded to the handler installed at g_pfnActorCutsceneSubstateUpdate. The
   block based at g_anActorCutsceneTimerBlock holds the whole transition: [-1]
   is the substate, [0] a frame timer advanced by g_nFrameStep, and the
   +0x34..+0x54 fields are the camera fly-in's start/goal triples (yaw, orbit
   radius, height).

   While the timer is under 0x100 the camera orbits the incoming level's
   entry point: a sine of the timer ramped over the window gives a 0..0x2000
   ease factor, that factor interpolates yaw the short way round the circle
   (AbsAngleDelta12 added or subtracted depending on which side of 0x800 the
   difference falls), and radius/height are lerped the same way. The camera is
   then placed on that orbit around the record at g_pActorCutsceneRecord +0xC
   and aimed back at it. For the first 0x40 frames the aim is a ramped approach
   — the pitch/yaw error is scaled by the timer itself and folded in — and after
   that it snaps straight to the computed angles.

   Once the timer passes 0x100 and the CD stream has reached state 0xA the
   handoff runs: the world counters are copied over in one block, substate
   advances to 5 with the timer reset, and Spyro plus the camera are parked on
   the entry point taken from g_anLevelEntryPoints (one 16-byte entry per level,
   the fourth word holding the facing byte). Every path falls through to the
   shared anim tick.

   MATCHED at 338/338 instructions.

   FOUR LEVERS GOT THE LAST 14 INSTRUCTIONS. Two of them are the repo's
   existing §B-i tool applied twice; the other two are new. What the rtl dump
   added is WHICH PASS does the folding -- see THE MECHANISM below.

   1. g_pActorCutsceneRecord read DIRECTLY at every use, in both arms (no `rec`
   local). The original reloads it into a CALLER-saved temp at each use, because
      every use is separated from the last by a `jal`; holding it emitted the
      load twice and cost a callee-saved register. Direct reads reproduce all
      7 materialisations register for register.

   2/3. TWO empty `do { } while (0);` dials -- one just before the substate
      test, one as the first statement of the handoff arm. This is the
      established §B-i ebb break (see ProbeSpyroGroundContact and eight other
      matched sources): the empty loop leaves a NOTE_INSN_LOOP_BEG that ends
      cse's extended basic block, so the base has no constant equivalence in
      the block below and its accesses keep `off(sN)`. New here is only that
      this function needs TWO, at those exact points. Together: 352 -> 340
      instructions, 14 %hi(g_anActorCutsceneTimerBlock) -> 4 (the original's
   count), and the frame goes back to 0x50 with four callee-saved registers
   (s0-s3). Placement is exact: the same dial before the APPROACH arm gives 354.

   4. TWO named locals, `lvl` and `face`, each read in its own statement.
      This is the subtle one and it is a different mechanism from the dials --
      see the rtl note below.

   THE MECHANISM (cse.c / explow.c, and the rtl dumps in build/rtl/).
     - expand force_regs every constant address (explow.c:396 memory_address,
       "By passing constant addresses thru registers we get a chance to cse
       them"), giving `(set (reg N) (const (plus sym K)))` + `(mem (reg N))`.
     - cse.c:2637 find_best_addr, the only thing that would rewrite a MEM's
       address in place, returns unconditionally on CONSTANT_ADDRESS_P. So an
       address that reaches it as a constant can NEVER become base+offset.
     - cse rewrites the force_reg'd SETs to `(plus base K)` correctly (14 of
       them here). cse2 -- the rerun after loop, enabled by -O2 via
       -frerun-cse-after-loop -- still knows the base equals the symbol and
       folds them all back. Recompiling the same .i: 14 refs with the locked
       flags, 2 with -fno-rerun-cse-after-loop.
     - cse2 can only fold where it knows the base pseudo's constant value in
       the current extended basic block. Hence the dials: the NOTE_INSN_LOOP_BEG
       an empty do/while leaves behind ends the ebb, and cse2 enters the block
       below with nothing to fold against. This refines the existing §B-i note,
       which reads the break as denying find_best_addr a constant equivalence:
       find_best_addr is not involved -- it returns before looking. The pass to
       blame is the post-loop cse rerun, and cse1's output is already correct.
     - Isolated on a 6-line test case: for a static symbol, only offset 0 keeps
       the base; every nonzero offset folds, negative as well as positive.

   WHY THE NAMED LOCALS ARE A SEPARATE FIX. `lvl` and `face` are NOT about
   cse2. Their reads were never force_reg'd in the first place, so they never
   reached cse's rewrite at all -- the second read of
   g_anActorCutsceneTimerBlock[4] came out of expand as `(set (reg 204) (mem
   (const (plus sym 16))))`, address inline, while the first read of the SAME
   field came out as
   `(set (reg 197) (const (plus sym 16)))` + `(mem (reg 197))`. Reading the
   value in its own statement puts it back on the force_reg path. Spelling the
   array access differently does NOT work -- `(&A[i])[3]`, `*(A + i + 3)` and
   `A[i + 3]` all parse to the same tree and produce byte-identical output.
   `face` does the same job for the tail: it pins the g_pActorCutsceneRecord
   load ahead of the g_anCameraPos load so the latter fills its shadow and the
   `lbu` needs no nop.

   HISTORY WORTH KEEPING. Every source-shape lever tried before the rtl dump
   failed, and the mechanism says they had to: a block-scoped pointer local
   (const-propagates), sizing the array, source reordering, naming the
   CopyVector args, a struct view with `t->field`, and an address-of pin all
   produce a pseudo whose constant value cse2 can see. The 2026-08-15 permuter
   run had already stumbled onto a do/while dial here without recognising it as
   the §B-i tool, and discarded it because its best candidate overshot to 6
   SHORT -- a reminder that permuter score is not this repo's metric.

   THE SIBLING. ActorCutscene_Draw is parked on the same fold over the same
   block, and its ~15-minute permuter run produced no improving candidate at
   all — which now reads as expected rather than discouraging, since the
   permuter can only stumble onto the dial by luck. Try the levers here in
   order: count its %hi(g_anActorCutsceneTimerBlock) against the original first
   (the difference times one is usually the whole residue), then place a
   do/while dial on whichever arm folds, then look for reads expand never
   force_regs. */

/* Held-base view of the gamestate-0xC block: [-1] substate, [0] timer,
   [13]/[15] the yaw and height starts also reachable as
   g_nActorCutsceneCamYawStart / g_nActorCutsceneCamHeightStart, [25] the active
   level record. */
/* g_nActorCutsceneTimer is the absolute alias of
   g_anActorCutsceneTimerBlock[0]: the aim ramp reads the timer after the camera
   calls, where the original drops the held base and re-addresses it. */

/* 20-byte block view for the counter handoff, which the original does as one
   struct assignment (lw/lw + sw/sw pairs), not five scalar copies. */
typedef struct {
  int words[5];
} WORLDCOUNTERS;

void ActorCutscene_Update(void) {
  VECTOR delta;
  SVECTOR aim;
  VECTOR settle;
  int ease;
  int yaw;
  int radius;
  int goal;
  int start;

  g_anActorCutsceneTimerBlock[0] += g_nFrameStep;
  RotateLightVectorXZ(3);

  /* LOAD-BEARING — do not delete, and do not "simplify". The §B-i ebb break:
     this empty loop emits no code, but its NOTE_INSN_LOOP_BEG ends cse's
     extended basic block, so the cse rerun after loop enters the block below
     with no constant equivalence for the g_anActorCutsceneTimerBlock base and
     cannot fold its accesses to absolute. Deleting it costs ~12 instructions
     and the fourth callee-saved register. Header has the pass-by-pass evidence.
   */
  EBB_BREAK();

  if (g_anActorCutsceneTimerBlock[-1] == 4) {
    if (g_nCdStreamState < LSTREAM_RELOCATE) {
      TickLevelTransitionStream(1);
    }

    if (g_anActorCutsceneTimerBlock[0] < 0x100) {
      /* Block-scoped per arm on purpose: one function-scope `cam` spanning both
         arms is a single pseudo with a single live range, and it holds a fifth
         callee-saved register the original does not use. */
      int *cam;

      /* 0..0x2000 ease: the sine argument sweeps -0x400..+0x400 across the
         0x100-frame window, so the factor runs a full half-cosine of travel.
         Both shifts are in the source — the sra cannot come from a divide. */
      ease = LookupSine(((g_anActorCutsceneTimerBlock[0] << 11) >> 8) - 0x400) +
             0x1000;

      /* goal/start are named so both feed the compare AND survive as the call
         arguments: the difference test below leaves them in a0/a1 exactly
         where AbsAngleDelta12 wants them, so the call needs no setup. */
      goal = g_nActorCutsceneCamYawGoal;
      start = g_nActorCutsceneCamYawStart;
      if (((goal - start) & 0xFFF) < 0x801) {
        yaw = g_nActorCutsceneCamYawStart +
              ((AbsAngleDelta12(goal, start) * ease) >> 13);
      } else {
        /* Same variable as `start`, but reached through the held base — the
           original re-addresses it here rather than reloading the symbol. */
        yaw = g_anActorCutsceneTimerBlock[13] -
              ((AbsAngleDelta12(goal, start) * ease) >> 13);
      }

      radius =
          g_nActorCutsceneCamRadiusStart +
          (((g_nActorCutsceneCamRadiusGoal - g_nActorCutsceneCamRadiusStart) *
            ease) >>
           13);
      cam = g_anCameraPosBlock;

      cam[0] =
          g_pActorCutsceneRecord->posX + ((LookupCosine(yaw) * radius) >> 12);
      g_anCameraPos[1] =
          g_pActorCutsceneRecord->posY + ((LookupSine(yaw) * radius) >> 12);
      g_anCameraPos[2] =
          g_nActorCutsceneCamHeightStart +
          (((g_nActorCutsceneCamHeightGoal - g_nActorCutsceneCamHeightStart) *
            ease) >>
           13);

      SubtractVector(&delta.vx, &g_pActorCutsceneRecord->posX, cam);
      delta.vz += 0x200;

      if (g_nActorCutsceneTimer < 0x40) {
        aim.vx = 0;
        aim.vy = ArcTan2(VectorLength(&delta.vx, VEC_LEN_XY), -delta.vz, 1);
        aim.vz = ArcTan2(delta.vx, delta.vy, 1);
        /* Unsigned view for the accumulate only: the call argument above is a
           signed read of the same halfword (lh), the fold back in is not. */
        g_nCameraEulerPitch =
            (*(unsigned short *)&g_nCameraEulerPitch +
             ((SignedAngleDelta12(aim.vy, g_nCameraEulerPitch) *
               g_nActorCutsceneTimer) >>
              6)) &
            0xFFF;
        g_nCameraEulerYaw = (*(unsigned short *)&g_nCameraEulerYaw +
                             ((SignedAngleDelta12(aim.vz, g_nCameraEulerYaw) *
                               g_nActorCutsceneTimer) >>
                              6)) &
                            0xFFF;
      } else {
        g_nCameraEulerPitch =
            ArcTan2(VectorLength(&delta.vx, VEC_LEN_XY), -delta.vz, 1);
        g_nCameraEulerYaw = ArcTan2(delta.vx, delta.vy, 1);
      }
    } else if (g_nCdStreamState == LSTREAM_RELOCATE) {
      int *cam; /* see the approach arm: block-scoped per arm on purpose */
      int face;
      int lvl;

      /* LOAD-BEARING — see the dial before the substate test above. This one
         does the same job for the handoff arm, which is where the base folds
         hardest (13 of the 14). */
      EBB_BREAK();

      *(WORLDCOUNTERS *)&g_nWorldRenderMeshChunkCount =
          *(WORLDCOUNTERS *)&g_nWorldCollisionTriCount;

      g_anActorCutsceneTimerBlock[-1] = 5;
      g_anActorCutsceneTimerBlock[0] = 0;
      CopyVector((int *)((char *)g_anActorCutsceneTimerBlock[25] + 0xC),
                 &g_anLevelEntryPoints[g_anActorCutsceneTimerBlock[4] * 4]);

      /* Read in its own statement rather than inline in the index below. This
         is the one read of this field that expand does NOT force_reg — it comes
         out as (set (reg) (mem (const (plus sym 16)))) with the address inline,
         where find_best_addr refuses to touch it, so it can never pick up the
         block base. Naming it puts it back on the force_reg path. Note the
         first read of the SAME field, in the CopyVector call above, is
         force_reg'd fine; spelling this one differently does not help, as
         A[i + 3], (&A[i])[3] and *(A + i + 3) all parse to one tree. */
      lvl = g_anActorCutsceneTimerBlock[4];
      g_pActorCutsceneRecord->rotZ = (char)g_anLevelEntryPoints[lvl * 4 + 3];
      CopyVector(g_anSpyroWorldPos,
                 (int *)((char *)g_anActorCutsceneTimerBlock[25] + 0xC));
      g_nSpyroWorldPosZ += 0x2D4;

      g_abSpyroPersistentEuler[2] = g_pActorCutsceneRecord->rotZ + 0x80;

      /* Written back to the block before the cosine call so the angle is read
         once and reused as the sine argument. */
      g_anActorCutsceneTimerBlock[19] =
          ((g_pActorCutsceneRecord->rotZ << 4) + 0x390) & 0xFFF;

      cam = g_anCameraPosBlock;
      cam[0] = g_pActorCutsceneRecord->posX +
               ((LookupCosine(g_anActorCutsceneTimerBlock[19]) *
                 g_anActorCutsceneTimerBlock[20]) >>
                12);
      g_anCameraPos[1] = g_pActorCutsceneRecord->posY +
                         ((LookupSine(g_anActorCutsceneTimerBlock[19]) *
                           g_anActorCutsceneTimerBlock[20]) >>
                          12);
      g_anCameraPos[2] = g_pActorCutsceneRecord->posZ + 0x200;

      SubtractVector(&settle.vx,
                     (int *)((char *)g_anActorCutsceneTimerBlock[25] + 0xC),
                     cam);
      settle.vz += 0x200;

      g_nCameraEulerPitch =
          ArcTan2(VectorLength(&settle.vx, VEC_LEN_XY), -settle.vz, 1);
      g_nCameraEulerYaw = ArcTan2(settle.vx, settle.vy, 1);

      /* Named to pin the g_pActorCutsceneRecord load ahead of the g_anCameraPos
         reads below, so the camera load falls into its shadow and the `lbu`
         needs no nop. Left inline in the [13] store, the scheduler hoists
         g_anCameraPos above the g_nCameraEulerYaw store instead and the nop
         costs one. */
      face = g_pActorCutsceneRecord->rotZ;
      g_anActorCutsceneTimerBlock[21] = g_anCameraPos[2];
      g_anActorCutsceneTimerBlock[15] = g_anCameraPos[2] - 0xFCE;
      g_anActorCutsceneTimerBlock[13] = ((face << 4) + 0xBA0) & 0xFFF;
    }
  } else {
    g_pfnActorCutsceneSubstateUpdate();
  }

  AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
  TickSpyroAnimLayer1();
  TickSpyroAnimLayer2();
}
