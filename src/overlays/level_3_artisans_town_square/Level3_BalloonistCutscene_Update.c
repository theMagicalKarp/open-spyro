/* Level3_BalloonistCutscene_Update (0x8007B0AC, level_3_artisans_town_square overlay).
 *
 * Balloonist cutscene tick — the per-frame update half of gamestate 0xC
 * (Level0_BalloonistCutscene_Draw is the draw half). Dispatches on the substate word
 * g_nGamestate0cSubstate through an 8-entry jump table (jtbl_8007AA3C):
 *   0  approach: swing the camera round to the balloonist over 0x60 frames,
 *   1  dialogue: pad-driven page/roster menu (g_nGamestate0cDialogPage submode),
 *   2  board:    fly Spyro to the balloon over 0x66 frames,
 *   3  lift off: raise the balloon (Z) for 0x80 frames, then snapshot the
 *                actor into g_abGamestate0cActorSnapshot and swap the 0x4000-byte scratch bank,
 *   5  cruise:   interpolate the camera along the flight path (0x100 frames),
 *   6  descend:  lower the balloon for 0x80 frames,
 *   7  land:     ease Spyro out of the balloon, then hand the frame back to
 *                gamestate 0 (InitLightVectorConstants).
 * Substate 4 is the "wait for the level stream" state and falls straight
 * through to the epilogue.  Identical body in the six hub overlays.
 *
 * Blocks are addressed through their splat symbols declared as incomplete
 * arrays and indexed with constants, so cse's related_value derives the
 * sibling offsets off one held base (A36) — that is what produces the
 * original's `lw 0(s0)` / `lw -0x18(s0)` pairs off g_nGamestate0cCamYawGoal and the
 * `addiu a1,s1,0x20` / `addiu s1,s1,0x14` derivations off g_nGamestate0cTimer.
 * A pointer local (`int *rot = g_nGamestate0cCamYawGoal; rot[-6]`) is the trap here: it
 * gives the pseudo a qty_const and B-i rule 1 folds every nonzero offset
 * back to absolute. Dropping the pointer locals for direct constant-index
 * access on the SAME symbol was worth 42 insns on its own.
 *
 * Clone of Level0_BalloonistCutscene_Update (matched 2026-09-14, 8,096 B).  Read the levers before editing -- several
 * statements look odd and every one is load-bearing.
 *
 * LEVERS (in the order they fell):
 *  L1  R6 shared s0 base.  `t = g_nGamestate0cDialogPage[0]` (ARRAY form) at the arm-1 head
 *      materialises s0 = &g_nGamestate0cDialogPage and cse follow-jumps carries it into the
 *      0x1F / 0x6840 arms, so `g_nGamestate0cDialogPage[-1]`/`[-2]` come out -4(s0)/-8(s0).
 *      Every store to F4 in arm 1 is `g_nGamestate0cDialogPage[0] = ...`; the EC reads/writes
 *      in the 0x1F arm are the `_s` scalar (absolute).
 *  L2  MEM_IN_STRUCT alias dial (sched anti-dependence).  A scalar store and
 *      an in_struct load at a varying address "cannot alias", so sched moves
 *      the load across the store.  Where the original keeps the load BELOW a
 *      store, spell the store so it CAN alias: array form `g_nGamestate0cTimer[0] = 0`
 *      (arm 1), `*((int *) ((char *) g_anGamestate0cSpyroFrom + 12)) = 0x4A0`, and
 *      `*((int *) ((char *) g_pCameraAnchorPos)) = ...` (the char* cast matters).
 *  L3  R1 cross-jump -- SOLVED, and not by registers.  Arm 3 (t >= 0x48) writes
 *      its `g_bSpyroPersistentEulerZ = k >> 4; g_nSpyroAnimPlayRate = 8;` tail INSIDE both branches,
 *      each preceded by an empty do/while(0) (so sched keeps the tail order);
 *      arm 1 keeps its tail after the join.  jump.c then merges arm 3's
 *      branch tails into a label it CREATES (uid >= max_uid), so path (b) can
 *      never pair arm 1's then-jump with arm 3's -- exactly the original's
 *      "identical but unmerged then-blocks, merged else-blocks".
 *  L4  k register in the arm-1/arm-3 accumulators: arm 3 uses a block-local
 *      `int k` per branch (local-alloc ties k to the dying load -> v0); arm 1
 *      keeps a shared k but routes the shifted term through `int m` declared
 *      at the arm-1 block (used in both branches => GLOBAL, so the load wins v0
 *      in local-alloc and m falls to v1).
 *  L5  Per-arm sn: arm 2 has its own `int sn` (local => local-alloc gives it
 *      s0 before &EC[5]'s pseudo), so base = s1, sn = s0 across case 0.
 *  L6  Block-local temps for the g_bSpyroAnimFrame -> g_bSpyroFramePrev copy (`int m`) and
 *      for `n` / `z` in the arm-3 re-seed and case-5 else blocks: a shared
 *      case-level variable is GLOBAL and loses every local-alloc race.
 *  L7  `k = g_nGamestate0cTimer[0];` after the `t == n` block in case 6 reproduces the
 *      original's `move v1,a0` join copy; `n -= 0x1B; g_nGamestate0cCardVariant = n;` and
 *      `vecD[2] += 0x200; vecD[2] -= cn >> 4;` are in-place forms.
 *  L8  DECODE fixes: `(k == 3) && (g_nLevelEggsRecovered12 < 2)` (was >= 2); the arm-1
 *      mode tests are `g_nGamestate0cDialogPage[0] == 0xE` / `g_nGamestate0cDialogPage[0] = 0x1E` (were
 *      F0 -- invisible to an immediate-masked diff, found by the linked diff);
 *      `g_nGamestate0cCamYawGoal[0]` (array) in both yaw conditions AND call args.
 *  L9  Case 6's else arm (`k >= 0x40`): the original's `0x400 - (t << 4)` reads
 *      t while `k - n` reads the k copy.  cse1 path variants that include the
 *      inner join pick t as canonical only if k's LAST use lies inside that
 *      path, so the outer else gets its own block-local `int k`, the else head
 *      gets an empty do/while(0) (cse1 barrier), and a dead `if (k < 0x40)`
 *      after the store gives k a last use past the join that only cse2 (which
 *      knows k >= 0x40 on that path) folds away.
 *  L10 Case 3's `t == n` block: `yaw` is set twice (`yaw += 0x2D4;
 *      g_nSpyroWorldPosZ = yaw;`).  A single-set yaw gets sched1's birthing boost and
 *      lands after the A77 load, which gives the g_pGamestate0cRecord pointer one insn
 *      too much life and loses it a1 to `z` in local-alloc.
 */

#include "level_globals.h"
#include "level_funcs.h"
#include "level_balloonist_cutscene.h"


void Level3_BalloonistCutscene_Update(void)
{
  int vecA[4];  /* sp+0x10 (PSX VECTOR: 16 bytes) */
  short ang[4]; /* sp+0x20 (SVECTOR: 8 bytes) */
  int vecB[4];  /* sp+0x28 */
  int vecC[4];  /* sp+0x38 */
  int vecD[4];  /* sp+0x48 */
  int vecE[4];  /* sp+0x58 */
  short pad[4]; /* sp+0x68, dead — sizes the frame to 0x88 */
  int *st;
  st = g_nGamestate0cSubstate;
  if (st[0] < 4)
  {
    g_pSaveMenuAnimActorList1 = 0;
    g_apSaveMenuAnimActorList[0] = g_pGamestate0cActor;
    TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, 0);
  }
  switch (st[0])
  {
    case 0:
    {
      int t;
      int cn;
      int sn;
      int k;
      int n;
      int yaw;
      int cz;

        t = g_nGamestate0cTimer[0];
        if (t < 0x60)
      {
        cn = LookupSine(((t << 11) / 96) - 0x400) + 0x1000;
        if (g_nGamestate0cTimer[0] == g_nFrameStep)
        {
          if (g_pLevelOverlayInitRecord != 0)
          {
            ACTOR_RAW(g_pLevelOverlayInitRecord, unsigned char, cullRadius) = 0;
            ACTOR_RAW(g_pLevelOverlayInitRecord, unsigned char, drawn) = 0;
            if (((int **) g_pLevelOverlayInitRecord)[0][3] != 0)
            {
              ClearWord(((int **) g_pLevelOverlayInitRecord)[0][3]);
              ((int **) g_pLevelOverlayInitRecord)[0][3] = 0;
            }
          }
          g_nSpyroFlameBreathTimerSave = g_nSpyroFlameBreathTimer;
          ResetSpyroEntity(1);
          g_bSpyroAnimPrev[0] = 1;
          g_bSpyroAnimCurrent = 1;
          g_bSpyroAnimFrame = 1;
          CopyVector(g_anGamestate0cSpyroFrom, &g_bSpyroAnimPrev[-0x18]);
          *((int *) ((char *) g_anGamestate0cSpyroFrom + 12)) = 0x4A0;
          g_nGamestate0cSpyroToY = 0;
          g_nGamestate0cSpyroToZ = 0x164;
          RotateVectorByMatrix(CUTSCENE_ACTOR->mtx, &g_anGamestate0cSpyroFrom[3], &g_anGamestate0cSpyroFrom[3]);
          AddVector(&g_anGamestate0cSpyroFrom[3], &g_anGamestate0cSpyroFrom[3], &CUTSCENE_ACTOR->posX);
          g_nGamestate0cSpyroYawStart = g_bSpyroPersistentEulerZ * 0x10;
          SubtractVector(vecA, &g_anGamestate0cSpyroFrom[3], g_anGamestate0cSpyroFrom);
          g_nGamestate0cSpyroYawGoal = ArcTan2(vecA[0], vecA[1], 1);
        }
        t = g_nGamestate0cTimer[0];
        if (t < 0x18)
        {
          int m;
          sn = LookupSine(((t << 11) / 24) - 0x400) + 0x1000;
          if (((g_nGamestate0cSpyroYawGoal - g_nGamestate0cSpyroYawStart) & 0xFFF) < 0x801)
          {
            m = (AbsAngleDelta12(g_nGamestate0cSpyroYawGoal, g_nGamestate0cSpyroYawStart) * sn) >> 13; k = g_nGamestate0cSpyroYawStart + m;
          }
          else
          {
            m = (AbsAngleDelta12(g_nGamestate0cSpyroYawGoal, g_nGamestate0cSpyroYawStart) * sn) >> 13; k = g_nGamestate0cSpyroYawStart - m;
          }
          g_bSpyroPersistentEulerZ = k >> 4;
          g_nSpyroAnimPlayRate = 8;
        }
        else
          if (t < 0x48)
        {
          int sn;
          sn = LookupSine((((t - 0x18) << 11) / 48) - 0x400) + 0x1000;
          SubtractVector(vecA, &g_nGamestate0cTimer[8], &g_nGamestate0cTimer[5]);
          ScaleVector3Sat(vecA, vecA, sn);
          RShiftVector3(vecA, 0xD);
          AddVector(g_anSpyroWorldPos, &g_nGamestate0cTimer[5], vecA);
          g_nSpyroAnimPlayRate = VectorLength(vecA, 0) >> 1;
          if (g_nSpyroAnimPlayRate < 4)
          {
            g_nSpyroAnimPlayRate = 4;
          }
          if (g_nSpyroAnimPlayRate >= 0x11)
          {
            g_nSpyroAnimPlayRate = 0x10;
          }
        }
        else
        {
          sn = LookupSine((((t - 0x48) << 11) / 24) - 0x400) + 0x1000;
          if ((g_nGamestate0cTimer[0] - g_nFrameStep) < 0x48)
          {
            int m;
            int n;
            g_nGamestate0cSpyroYawStart = g_bSpyroPersistentEulerZ * 0x10;
            n = ACTOR_RAW(g_pGamestate0cActor, unsigned char, rotZ);
            m = g_bSpyroAnimFrame;
            g_bSpyroAnimCurrent = 3;
            g_bSpyroFramePrev = m;
            g_bSpyroAnimFrame = 0;
            g_bSpyroSubFrameTimer = 0;
            g_nSpyroAnimPlayRate = 8;
            g_nGamestate0cSpyroYawGoal = ((n * 0x10) - 0x800) & 0xFFF;
          }
          if (((g_nGamestate0cSpyroYawGoal - g_nGamestate0cSpyroYawStart) & 0xFFF) < 0x801)
          {
            int k; k = g_nGamestate0cSpyroYawStart + ((AbsAngleDelta12(g_nGamestate0cSpyroYawGoal, g_nGamestate0cSpyroYawStart) * sn) >> 13);
            do { } while (0);
            g_bSpyroPersistentEulerZ = k >> 4;
            g_nSpyroAnimPlayRate = 8;
          }
          else
          {
            int k; k = g_nGamestate0cSpyroYawStart - ((AbsAngleDelta12(g_nGamestate0cSpyroYawGoal, g_nGamestate0cSpyroYawStart) * sn) >> 13);
            do { } while (0);
            g_bSpyroPersistentEulerZ = k >> 4;
            g_nSpyroAnimPlayRate = 8;
          }
        }
        g_abSpyroPersistentEuler[0] = 0;
        g_bSpyroPersistentEulerY = 0;
        g_nSpyroGroundHeightZ = FindGroundHeightBelow((int *) (&g_abSpyroPersistentEuler[-0xC]), 0x10000);
        ApplyEulerRotation((int *) (&g_abSpyroPersistentEuler[0]), (int *) (&g_abSpyroPersistentEuler[0x28]), 0);
        SampleSpyroShadowRingHeights(1);
        TickSpyroAttackEffects(g_nFrameStep);
        if (((g_nGamestate0cCamYawGoal[0] - g_nGamestate0cCamYawGoal[-6]) & 0xFFF) < 0x801)
        {
          yaw = g_nGamestate0cCamYawGoal[-6] + ((AbsAngleDelta12(g_nGamestate0cCamYawGoal[0], g_nGamestate0cCamYawGoal[-6]) * cn) >> 13);
        }
        else
        {
          yaw = g_nGamestate0cCamYawGoal[-6] - ((AbsAngleDelta12(g_nGamestate0cCamYawGoal[0], g_nGamestate0cCamYawGoal[-6]) * cn) >> 13);
        }
        cz = g_nGamestate0cCamRadiusStart + (((g_nGamestate0cCamRadiusGoal - g_nGamestate0cCamRadiusStart) * cn) >> 13);
        g_anCameraPos[0] = ACTOR_RAW(CUTSCENE_ACTOR, int, posX) + ((LookupCosine(yaw) * cz) >> 12);
        g_nCameraPosY = ACTOR_RAW(CUTSCENE_ACTOR, int, posY) + ((LookupSine(yaw) * cz) >> 12);
        vecA[0] = 0x220;
        vecA[2] = 0x340;
        vecA[1] = 0;
        g_nCameraPosZ = g_nGamestate0cCamHeightStart + (((g_nGamestate0cCamHeightGoal - g_nGamestate0cCamHeightStart) * cn) >> 13);
        RotateVectorByMatrix(CUTSCENE_ACTOR->mtx, vecA, vecA);
        AddVector(vecA, vecA, &CUTSCENE_ACTOR->posX);
        SubtractVector(vecA, vecA, g_anCameraPos);
        if (g_anGamestate0cTimerBlock_s < 0x20)
        {
          ang[0] = 0;
          ang[1] = ArcTan2(VectorLength(vecA, 0), -vecA[2], 1);
          ang[2] = ArcTan2(vecA[0], vecA[1], 1);
          g_nCameraEulerRoll = (((unsigned short) g_nCameraEulerRoll) + ((SignedAngleDelta12(ang[0], g_nCameraEulerRoll) * g_anGamestate0cTimerBlock_s) >> 5)) & 0xFFF;
          g_nCameraEulerPitch = (((unsigned short) g_nCameraEulerPitch) + ((SignedAngleDelta12(ang[1], g_nCameraEulerPitch) * g_anGamestate0cTimerBlock_s) >> 5)) & 0xFFF;
          g_nCameraEulerYaw = (((unsigned short) g_nCameraEulerYaw) + ((SignedAngleDelta12(ang[2], g_nCameraEulerYaw) * g_anGamestate0cTimerBlock_s) >> 5)) & 0xFFF;
        }
        else
        {
          g_nCameraEulerRoll = 0;
          g_nCameraEulerPitch = ArcTan2(VectorLength(vecA, 0), -vecA[2], 1);
          g_nCameraEulerYaw = ArcTan2(vecA[0], vecA[1], 1);
        }
        g_pfnLevelAdvanceEmitList(g_nFrameStep);
        TickActiveSoundVoices();
      }
      else
      {
        g_anCameraPos[0] = ACTOR_RAW(CUTSCENE_ACTOR, int, posX) + ((LookupCosine(g_nGamestate0cCamYawGoal_s) * g_nGamestate0cCamRadiusGoal) >> 12);
        g_nCameraPosY = ACTOR_RAW(CUTSCENE_ACTOR, int, posY) + ((LookupSine(g_nGamestate0cCamYawGoal_s) * g_nGamestate0cCamRadiusGoal) >> 12);
        g_nCameraPosZ = g_nGamestate0cCamHeightGoal;
        CopyVector(g_anSpyroWorldPos, &g_nGamestate0cTimer[8]);
        n = g_bSpyroAnimFrame;
        g_nSpyroAnimPlayRate = 4;
        g_bSpyroAnimCurrent = 0;
        g_bSpyroAnimFrame = 0;
        g_bSpyroSubFrameTimer = 0;
        g_nGamestate0cSubstate[0] = 1;
        g_nGamestate0cTimer[0] = 0;
        g_bSpyroPersistentEulerZ = g_nGamestate0cSpyroYawGoal >> 4;
        g_bSpyroFramePrev = n;
      }
        break;
    }

    case 1:
    {
      int t;
      int n;
      int k;
      int i;

        t = g_nGamestate0cDialogPage[0];
        if (t < 0x1E)
      {
        if ((g_dwPadPressed[0] & 0x6840) && (g_anGamestate0cTimerBlock_s >= 0x51))
        {
          n = t % 6;
          if (n == 0)
          {
            g_nGamestate0cDialogPage[0] = t + 1;
          }
          else
            if (n < 3)
          {
            if ((t < 3) && (g_bWorldBalloonistState1 < 2))
            {
              g_pfnLevelOnSaveMenuExit();
            }
            else
              if (g_nGamestate0cDialogPage[0] == 0xE)
            {
              do
              {
                k = ((int) GetRandomU32()) % 5;
                if ((k == 0) && (g_nLevelEggsRecovered1 == 0))
                {
                  g_nGamestate0cDialogPage_s = 0x21;
                }
                else
                  if ((k == 1) && (g_nLevelEggsRecovered7 == 0))
                {
                  g_nGamestate0cDialogPage_s = 0x22;
                }
                else
                  if ((k == 2) && (g_nLevelEggsRecovered8 == 0))
                {
                  g_nGamestate0cDialogPage_s = 0x23;
                }
                else
                  if ((k == 3) && (g_nLevelEggsRecovered12 < 2))
                {
                  g_nGamestate0cDialogPage_s = 0x24;
                }
                else
                  if ((k == 4) && (g_nLevelEggsRecovered15 == 0))
                {
                  g_nGamestate0cDialogPage_s = 0x25;
                }
              }
              while (g_nGamestate0cDialogPage_s == 0xE);
            }
            else
            {
              g_nGamestate0cDialogPage[0] = 0x1E;
            }
          }
          else
            if (n == 3)
          {
            g_nGamestate0cDialogPage[0] = t + 2;
          }
          else
            if (n == 4)
          {
            g_nGamestate0cDialogPage[0] = t + 1;
          }
          else
          {
            g_nGamestate0cDialogPage[0] = 0x1F;
          }
          g_nGamestate0cTimer[0] = 0;
        }
      }
      else
        if (t == 0x1E)
      {
        i = 1;
        k = 2;
        scan:
        if (g_abWorldBalloonistState[i] == k)
        {
          do { i++; } while (0);
          if (i < 6)
          {
            goto scan;
          }
        }

        if (g_dwPadPressed[0] & 0x1000)
        {
          g_nGamestate0cMenuCursor[0] = g_nGamestate0cMenuCursor[0] - 1;
          if (g_nGamestate0cMenuCursor[0] < 0)
          {
            g_nGamestate0cMenuCursor[0] = i - 1;
          }
          g_nGamestate0cTimer[0] = 0x20;
          PlaySoundEffect(g_pLevelSampleBankHeader[0x2D], 0, 0x10, 0);
        }
        else
          if (g_dwPadPressed[0] & 0x4000)
        {
          g_nGamestate0cMenuCursor[0] = g_nGamestate0cMenuCursor[0] + 1;
          if (g_nGamestate0cMenuCursor[0] >= i)
          {
            g_nGamestate0cMenuCursor[0] = 0;
          }
          g_nGamestate0cTimer[0] = 0x20;
          PlaySoundEffect(g_pLevelSampleBankHeader[0x2D], 0, 0x10, 0);
        }
        if ((g_dwPadPressed[0] & 0x840) == 0)
        {
          return;
        }
        if (g_nGamestate0cTimer[0] < 0x1F)
        {
          return;
        }
        if (g_nGamestate0cTimer[1] == 0)
        {
          g_pfnLevelOnSaveMenuExit();
          return;
        }
        StopAllSoundExceptMask(0);
        g_nGamestate0cSubstate[0] = 2;
        g_nGamestate0cTimer[0] = 0;
        g_nCurrentLevelId = g_nGamestate0cTimer[1] * 0xA;
        if (g_nCurrentLevelId >= g_nActiveLevelId)
        {
          g_nCurrentLevelId = g_nCurrentLevelId + 0xA;
        }
        g_nGamestate0cCardVariant = (g_nCurrentLevelId / 10) - 1;
      }
      else
        if (((unsigned int) (t - 0x1F)) < 2)
      {
        if (g_dwPadPressed[0] & 0x5000)
        {
          g_anGamestate0cTimerBlock_s = 0x20;
          g_nGamestate0cMenuCursor_s = 1 - g_nGamestate0cMenuCursor_s;
          PlaySoundEffect(g_pLevelSampleBankHeader[0x2D], 0, 0x10, 0);
        }
        if ((g_dwPadPressed[0] & 0x840) == 0)
        {
          return;
        }
        if (g_anGamestate0cTimerBlock_s < 0x1F)
        {
          return;
        }
        if (g_nGamestate0cDialogPage[-1] == 0)
        {
          if (g_nGamestate0cDialogPage[0] == 0x1F)
          {
            g_pfnLevelOnSaveMenuExit();
            return;
          }
          g_nGamestate0cDialogPage[0] = 0x1E;
          g_nGamestate0cDialogPage[-1] = 0;
          g_nGamestate0cDialogVariant = 1;
          return;
        }
        StopAllSoundExceptMask(0);
        g_nGamestate0cSubstate[0] = 2;
        g_nGamestate0cTimer[0] = 0;
        if (g_nGamestate0cDialogPage[0] == 0x1F)
        {
          g_nCurrentLevelId = g_nActiveLevelId + 0xA;
          g_nGamestate0cCardVariant = (g_nCurrentLevelId / 10) - 1;
        }
      }
      else
        if ((g_dwPadPressed[0] & 0x6840) && (g_nGamestate0cDialogPage[-2] >= 0x51))
      {
        if ((t == 0x24) || (t == 0x26))
        {
          g_nGamestate0cDialogPage[0] = 0x1E;
          g_nGamestate0cMenuCursor[0] = 0;
          g_nGamestate0cDialogVariant = 1;
          return;
        }
        if (t == 0x25)
        {
          g_nGamestate0cDialogPage[0] = 0x26;
          g_nGamestate0cDialogPage[-2] = 0;
          return;
        }
        if (t == 0x21)
        {
          g_nCurrentLevelId = 0xB;
        }
        else
          if (t == 0x22)
        {
          g_nCurrentLevelId = 0x15;
        }
        else
          if (t == 0x23)
        {
          g_nCurrentLevelId = 0x16;
        }
        n = g_nGamestate0cDialogPage_s;
        g_nGamestate0cDialogPage_s = 0x20;
        g_nGamestate0cTimer[0] = 0;
        n -= 0x1B;
        g_nGamestate0cCardVariant = n;
      }
        break;
    }

    case 2:
    {
      int t;
      int cn;
      int n;
      int yaw;
      int cz;

        t = g_nGamestate0cTimer[0];
        if (t < 0x66)
      {
        cn = LookupSine(((t << 11) / 102) - 0x400) + 0x1000;
        if (g_nGamestate0cTimer[0] == g_nFrameStep)
        {
          ACTOR_RAW(g_pGamestate0cRecord, unsigned char, visZone) = 0xFF;
          g_nGamestate0cCamYawStart = ArcTan2(g_anCameraPos[0] - ACTOR_RAW(CUTSCENE_RECORD, int, posX), g_nCameraPosY - ACTOR_RAW(CUTSCENE_RECORD, int, posY), 1);
          SubtractVector(vecB, g_anCameraPos, &CUTSCENE_RECORD->posX);
          g_nGamestate0cCamRadiusStart = VectorLength(vecB, 0);
          g_nGamestate0cCamHeightStart = g_nCameraPosZ;
          g_nGamestate0cCamEulerRollStart = g_nCameraEulerRoll;
          g_nGamestate0cCamEulerPitchStart = g_nCameraEulerPitch;
          g_nGamestate0cCamEulerYawStart = g_nCameraEulerYaw;
          n = ACTOR_RAW(g_pGamestate0cRecord, unsigned char, rotZ);
          g_nGamestate0cCamRadiusGoal = 0xE00;
          g_nGamestate0cCamYawGoal_s = ((n * 0x10) - 0x480) & 0xFFF;
          g_nGamestate0cCamHeightGoal = ACTOR_RAW(CUTSCENE_RECORD, int, posZ) + 0x200;
          g_nGamestate0cRecordZStart = ACTOR_RAW(CUTSCENE_RECORD, int, posZ);
          g_nGamestate0cRecordZGoal = ACTOR_RAW(CUTSCENE_ACTOR, int, posZ) + 0x488;
          CopyVector(&g_nGamestate0cTimer[5], g_anSpyroWorldPos);
          CopyVector(&g_nGamestate0cTimer[8], &CUTSCENE_ACTOR->posX);
          {
            int m;
          m = g_bSpyroAnimFrame;
          g_bSpyroAnimCurrent = 5;
          g_bSpyroAnimFrame = 0;
          g_bSpyroSubFrameTimer = 0;
          g_nSpyroAnimPlayRate = 7;
          g_bSpyroFramePrev = m;
          }
        }
        t = g_nGamestate0cTimer[0];
        if (t < 0x30)
        {
          g_anSpyroWorldPos[0] = g_anGamestate0cSpyroFrom[0] + (((g_nGamestate0cSpyroToX - g_anGamestate0cSpyroFrom[0]) * t) / 48);
          g_nSpyroWorldPosY = g_nGamestate0cSpyroFromY + (((g_nGamestate0cSpyroToY - g_nGamestate0cSpyroFromY) * t) / 48);
          g_nSpyroWorldPosZ = g_nGamestate0cSpyroFromZ + ((LookupSine(t << 5) * 0xA) / 36);
          g_nSpyroGroundHeightZ = FindGroundHeightBelow(g_anSpyroWorldPos, 0x10000);
          ApplyEulerRotation(&g_anSpyroWorldPos[3], &g_anSpyroWorldPos[13], 0);
          SampleSpyroShadowRingHeights(1);
        }
        else
        {
          if ((t - g_nFrameStep) < 0x30)
          {
            g_anGamestate0cSpyroFrom[0] = g_nGamestate0cTimer[8];
            g_nGamestate0cSpyroFromY = g_nGamestate0cSpyroToY;
            g_nGamestate0cSpyroFromZ = g_nGamestate0cSpyroFromZ + 0x325;
            CopyVector(&g_nGamestate0cTimer[8], &CUTSCENE_RECORD->posX);
            {
              int m;
            m = g_bSpyroAnimFrame;
            g_bSpyroAnimFrame = 0;
            g_bSpyroSubFrameTimer = 0;
            g_nSpyroAnimPlayRate = 4;
            g_bSpyroFramePrev = m;
            }
          }
          t = g_nGamestate0cTimer[0];
          if ((t - g_nFrameStep) >= 0x36)
          {
            t = t - 0x36;
            g_nSpyroAnimPlayRate = 7;
            g_anSpyroWorldPos[0] = g_anGamestate0cSpyroFrom[0] + (((g_nGamestate0cSpyroToX - g_anGamestate0cSpyroFrom[0]) * t) / 48);
            g_nSpyroWorldPosY = g_nGamestate0cSpyroFromY + (((g_nGamestate0cSpyroToY - g_nGamestate0cSpyroFromY) * t) / 48);
            g_nSpyroWorldPosZ = g_nGamestate0cSpyroFromZ + (LookupSine(t << 5) >> 2);
          }
        }
        if (((g_nGamestate0cCamYawGoal[0] - g_nGamestate0cCamYawGoal[-6]) & 0xFFF) < 0x801)
        {
          yaw = g_nGamestate0cCamYawGoal[-6] + ((AbsAngleDelta12(g_nGamestate0cCamYawGoal[0], g_nGamestate0cCamYawGoal[-6]) * cn) >> 13);
        }
        else
        {
          yaw = g_nGamestate0cCamYawGoal[-6] - ((AbsAngleDelta12(g_nGamestate0cCamYawGoal[0], g_nGamestate0cCamYawGoal[-6]) * cn) >> 13);
        }
        cz = g_nGamestate0cCamRadiusStart + (((g_nGamestate0cCamRadiusGoal - g_nGamestate0cCamRadiusStart) * cn) >> 13);
        g_anCameraPos[0] = ACTOR_RAW(CUTSCENE_RECORD, int, posX) + ((LookupCosine(yaw) * cz) >> 12);
        g_nCameraPosY = ACTOR_RAW(CUTSCENE_RECORD, int, posY) + ((LookupSine(yaw) * cz) >> 12);
        g_nCameraPosZ = g_nGamestate0cCamHeightStart + (((g_nGamestate0cCamHeightGoal - g_nGamestate0cCamHeightStart) * cn) >> 13);
        SubtractVector(vecB, &CUTSCENE_RECORD->posX, g_anCameraPos);
        ang[1] = ArcTan2(VectorLength(vecB, 0), -vecB[2], 1);
        ang[2] = ArcTan2(vecB[0], vecB[1], 1);
        g_nCameraEulerPitch = (g_nGamestate0cCamEulerPitchStart + ((SignedAngleDelta12(ang[1], g_nGamestate0cCamEulerPitchStart) * cn) >> 13)) & 0xFFF;
        g_nCameraEulerYaw = (g_nGamestate0cCamEulerYawStart + ((SignedAngleDelta12(ang[2], g_nGamestate0cCamEulerYawStart) * cn) >> 13)) & 0xFFF;
        ACTOR_RAW(CUTSCENE_RECORD, int, posZ) = g_nGamestate0cRecordZStart + (((g_nGamestate0cRecordZGoal - g_nGamestate0cRecordZStart) * cn) >> 13);
      }
      else
      {
        g_nGamestate0cSubstate[0] = 3;
        g_nGamestate0cTimer[0] = 0;
      }
        break;
    }

    case 3:
    {
      int z;
      int k;
      int yaw;
      int t;
      int n;
      int * bank;
      int step;
      int * slot;

        if (g_nGamestate0cTimer[0] < 0x80)
      {
        if (g_nGamestate0cTimer[0] == g_nFrameStep)
        {
          z = ACTOR_RAW(CUTSCENE_RECORD, int, posZ);
          k = g_bSpyroAnimFrame;
          g_nGamestate0cCamHeightStart = g_nCameraPosZ;
          g_anSpyroWorldPos[0] = g_nGamestate0cSpyroToX;
          g_nSpyroWorldPosY = g_nGamestate0cSpyroToY;
          g_nGamestate0cRecordZStart = z;
          yaw = ACTOR_RAW(CUTSCENE_RECORD, int, posZ);
          g_bSpyroFramePrev = k;
          g_nSpyroAnimPlayRate = 4;
          g_bSpyroAnimCurrent = 0;
          g_bSpyroAnimFrame = 0;
          g_bSpyroSubFrameTimer = 0;
          g_bSpyroOtBinBias = 3;
          yaw += 0x2D4;
          g_nSpyroWorldPosZ = yaw;
        }
        t = g_nGamestate0cTimer[0];
        if (t < 0x40)
        {
          ACTOR_RAW(CUTSCENE_RECORD, int, posZ) = g_nGamestate0cRecordZStart + (((LookupSine((t << 4) + 0xC00) + 0x1000) * 3) >> 2);
        }
        else
        {
          ACTOR_RAW(CUTSCENE_RECORD, int, posZ) = ACTOR_RAW(CUTSCENE_RECORD, int, posZ) + (g_nFrameStep * 0x4B);
          g_nCameraPosZ = g_nGamestate0cCamHeightStart + (((LookupSine((g_nGamestate0cTimer[0] << 4) + 0x800) + 0x1000) * 3) >> 2);
        }
        SubtractVector(vecC, &CUTSCENE_RECORD->posX, g_anCameraPos);
        g_nCameraEulerPitch = ArcTan2(VectorLength(vecC, 0), -vecC[2], 1);
        g_nCameraEulerYaw = ArcTan2(vecC[0], vecC[1], 1);
        g_nSpyroWorldPosZ = ACTOR_RAW(CUTSCENE_RECORD, int, posZ) + 0x2D4;
        g_bSpyroPersistentEulerZ = ACTOR_RAW(g_pGamestate0cRecord, unsigned char, rotZ) + 0x80;
      }
      else
      {
        g_nGamestate0cCamYawStart = ArcTan2(g_anCameraPos[0] - ACTOR_RAW(CUTSCENE_RECORD, int, posX), g_nCameraPosY - ACTOR_RAW(CUTSCENE_RECORD, int, posY), 1);
        SubtractVector(vecC, g_anCameraPos, &CUTSCENE_RECORD->posX);
        g_nGamestate0cCamRadiusStart = VectorLength(vecC, 0);
        g_nGamestate0cCamHeightStart = g_nCameraPosZ;
        n = ACTOR_RAW(g_pGamestate0cRecord, unsigned char, rotZ);
        g_nGamestate0cCamRadiusGoal = 0x800;
        g_nCdStreamState = 0;
        g_nPreserveMobIdx = -1;
        g_nScriptedRespawnFlag = 0;
        g_nScriptedMobTag = 0;
        g_nGamestate0cCamYawGoal_s = ((n * 0x10) + 0x390) & 0xFFF;
        {
          int m;
        m = ACTOR_RAW(CUTSCENE_RECORD, int, posZ);
        g_nGamestate0cSubstate[0] = 4;
        g_nGamestate0cTimer[0] = 0;
        g_nGamestate0cCamHeightGoal = m + 0x200;
        }
        TickLevelTransitionStream(1);
        CopyWords(g_abGamestate0cActorSnapshot, g_pGamestate0cRecord, 0x58);
        g_pGamestate0cRecord = (unsigned char *) g_abGamestate0cActorSnapshot;
        if (((unsigned int) g_pRenderScratchRegionBase) <= 0x801FFFFF)
        {
          bank = (int *) (g_pWorldRenderMeshChunkArray - 0x400C);
        }
        else
        {
          bank = (int *) 0x801FE000;
        }
        CopyWords(bank, g_apActorMeshTable_1A0[0], 0x4000);
        step = ((int) bank) - ((int) g_apActorMeshTable_1A0[0]);
        g_apActorMeshTable_1A0[0] = bank;
        g_apActorMeshTable_1A0[0][14] = g_apActorMeshTable_1A0[0][14] + step;
        slot = (int *) g_apActorMeshTable_1A0[0][14];
        slot[5] = slot[5] + step;
        slot = (int *) g_apActorMeshTable_1A0[0][14];
        slot[6] = slot[6] + step;
        slot = (int *) g_apActorMeshTable_1A0[0][14];
        slot[9] = slot[9] + step;
      }
        break;
    }

    case 5:
    {
      int t;
      int cn;
      int yaw;
      int cz;

        if (g_nCdStreamState < 0xD)
      {
        TickLevelTransitionStream(1);
      }
        t = g_nGamestate0cTimer[0];
        if (t < 0x100)
      {
        cn = LookupSine(((t << 11) >> 8) - 0x400) + 0x1000;
        if (((g_nGamestate0cCamYawStart - g_nGamestate0cCamYawGoal_s) & 0xFFF) < 0x801)
        {
          yaw = g_nGamestate0cCamYawGoal_s + ((AbsAngleDelta12(g_nGamestate0cCamYawStart, g_nGamestate0cCamYawGoal_s) * cn) >> 13);
        }
        else
        {
          yaw = g_nGamestate0cCamYawGoal_s - ((AbsAngleDelta12(g_nGamestate0cCamYawStart, g_nGamestate0cCamYawGoal_s) * cn) >> 13);
        }
        cz = g_nGamestate0cCamRadiusGoal + (((g_nGamestate0cCamRadiusStart - g_nGamestate0cCamRadiusGoal) * cn) >> 13);
        g_anCameraPos[0] = ACTOR_RAW(CUTSCENE_RECORD, int, posX) + ((LookupCosine(yaw) * cz) >> 12);
        g_nCameraPosY = ACTOR_RAW(CUTSCENE_RECORD, int, posY) + ((LookupSine(yaw) * cz) >> 12);
        g_nCameraPosZ = g_nGamestate0cCamHeightGoal + (((g_nGamestate0cCamHeightStart - g_nGamestate0cCamHeightGoal) * cn) >> 13);
        SubtractVector(vecD, &CUTSCENE_RECORD->posX, g_anCameraPos);
        vecD[2] += 0x200; vecD[2] -= cn >> 4;
        g_nCameraEulerPitch = ArcTan2(VectorLength(vecD, 0), -vecD[2], 1);
        g_nCameraEulerYaw = ArcTan2(vecD[0], vecD[1], 1);
      }
      else
        if (g_nCdStreamState == 0xD)
      {
        g_nGamestate0cSubstate[0] = 6;
        g_nGamestate0cTimer[0] = 0;
        TickLevelTransitionStream(1);
      }
        break;
    }

    case 6:
    {
      int t;
      int n;
      int k;

        t = g_nGamestate0cTimer[0];
        if (t < 0x80)
      {
        n = g_nFrameStep;
        if (t == n)
        {
          int z;
          z = ACTOR_RAW(CUTSCENE_RECORD, int, posZ);
          g_nGamestate0cCamHeightStart = g_nCameraPosZ;
          g_nGamestate0cRecordZStart = z;
        }
        k = g_nGamestate0cTimer[0];
        if (k < 0x40)
        {
          ACTOR_RAW(CUTSCENE_RECORD, int, posZ) = ACTOR_RAW(CUTSCENE_RECORD, int, posZ) - (n * 0x4B);
          g_nCameraPosZ = g_nGamestate0cCamHeightStart + ((LookupSine((-g_nGamestate0cTimer[0]) << 4) * 3) >> 2);
        }
        else
        {
          do
          {
          }
          while (0);
          if ((k - n) < 0x40)
          {
            g_nGamestate0cRecordZStart = ACTOR_RAW(CUTSCENE_RECORD, int, posZ) - 0x4B;
          }
          ACTOR_RAW(CUTSCENE_RECORD, int, posZ) = g_nGamestate0cRecordZStart + ((LookupSine(0x400 - (t << 4)) * 3) >> 2);
          if (k < 0x40) /* L9: dead; cse2 folds it */
          {
            g_nGamestate0cRecordZStart = 0;
          }
        }
        SubtractVector(vecE, &CUTSCENE_RECORD->posX, g_anCameraPos);
        g_nCameraEulerPitch = ArcTan2(VectorLength(vecE, 0), -vecE[2], 1);
        g_nCameraEulerYaw = ArcTan2(vecE[0], vecE[1], 1);
        g_nSpyroWorldPosZ = ACTOR_RAW(CUTSCENE_RECORD, int, posZ) + 0x2D4;
        g_bSpyroPersistentEulerZ = ACTOR_RAW(g_pGamestate0cRecord, unsigned char, rotZ) + 0x80;
      }
      else
      {
        g_nGamestate0cSubstate[0] = 7;
        g_nGamestate0cTimer[0] = 0;
        CopyVector(&g_nGamestate0cTimer[5], g_anSpyroWorldPos);
        {
          int m;
          int k;
        m = g_nGamestate0cCardVariant * 8;
        g_nGamestate0cTimer[8] = *((int *) (((char *) g_anGamestate0cLandingX) + m));
        k = *((int *) (((char *) g_anGamestate0cLandingY) + m));
        g_nGamestate0cSpyroToZ = g_nSpyroWorldPosZ;
        g_nGamestate0cSpyroToY = k;
        g_nGamestate0cSpyroToZ = FindGroundHeightBelow(&g_nGamestate0cTimer[8], 0x10000) + 0x164;
        k = g_bSpyroAnimFrame;
        g_bSpyroAnimCurrent = 5;
        g_bSpyroAnimFrame = 0;
        g_bSpyroSubFrameTimer = 0;
        g_nSpyroAnimPlayRate = 6;
        g_bSpyroFramePrev = k;
        }
      }
        break;
    }

    case 7:
    {
      int t;

        t = g_nGamestate0cTimer[0];
        if (t < 0x40)
      {
        g_anSpyroWorldPos[0] = g_anGamestate0cSpyroFrom[0] + (((g_nGamestate0cSpyroToX - g_anGamestate0cSpyroFrom[0]) * t) >> 6);
        g_nSpyroWorldPosY = g_nGamestate0cSpyroFromY + (((g_nGamestate0cSpyroToY - g_nGamestate0cSpyroFromY) * t) >> 6);
        g_nSpyroWorldPosZ = g_nGamestate0cSpyroToZ + ((LookupSine((t * 0x18) + 0x200) * (g_nGamestate0cSpyroFromZ - g_nGamestate0cSpyroToZ)) / 2896);
        g_nSpyroGroundHeightZ = FindGroundHeightBelow(g_anSpyroWorldPos, 0x10000);
        ApplyEulerRotation(&g_anSpyroWorldPos[3], &g_anSpyroWorldPos[13], 0);
        SampleSpyroShadowRingHeights(1);
        if (g_nGamestate0cTimer[0] >= 0x20)
        {
          ACTOR_RAW(CUTSCENE_RECORD, int, posZ) = ACTOR_RAW(CUTSCENE_RECORD, int, posZ) + (g_nGamestate0cTimer[0] * g_nFrameStep);
        }
      }
      else
      {
        CopyVector(g_anSpyroWorldPos, &g_nGamestate0cTimer[8]);
        ResetSpyroEntity(1);
        g_nSpyroPadSnapshotCountdown = 0x10;
        *((int *) ((char *) g_pCameraAnchorPos)) = (int) g_anSpyroWorldPos;
        g_nCameraCurrentMode = 0;
        g_nCameraNextMode = 0;
        g_pCameraTargetParams = (int) g_anCameraDynamicTargetParams;
        g_nCameraTargetYawBiasFromMode = g_nSpyroBodyYaw;
        ComputeCameraToTargetAngle(&g_pCameraAnchorPos[-0x2A]);
        CopyWords(g_anCameraDynamicTargetParams, &g_pCameraAnchorPos[-0x10], 0x18);
        g_anCameraDynamicTargetParams[0] = (g_anCameraDynamicTargetParams[0] + g_nCameraTargetYawBiasFromMode) & 0xFFF;
        ResetCameraStateToTarget();
        InitHudCounters(0);
        g_nGamestate = 0;
        g_nGamePaused = 1;
        g_nGameplayBlocked = 0;
        InitLightVectorConstants();
      }
        break;
    }

  }

}
