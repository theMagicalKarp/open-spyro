#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "pad_buttons.h"
#include "sample_bank.h"
#include "spyro_block.h"
#include "vecmath.h"

/* The save-menu record at 0x80078d00: [0] mode, [1] phase,
   [2] cursor, [3] substate, [9]/[10] memcard sync results, then from/to
   keyframes for the save host (+0x2C/+0x38), Sparx (+0x44/+0x4C),
   the camera (+0x54/+0x60) and Spyro's yaw (+0x6C/+0x70). */

#define SM g_anSaveMenuBlock
#define SPYB ((unsigned char *)g_anSpyroWorldPosBlock)
#define ACT g_pSaveMenuHostActor
#define FLD(p, off) (*(int *)((char *)(p) + (off)))

/* Eases an angle from `from` toward `to` by t/0x2000 along the short arc. */
#define LERP_ANGLE(to, from, t)                                                \
  ((((to) - (from)) & 0xFFF) < 0x801                                           \
       ? (from) + ((AbsAngleDelta12((to), (from)) * (t)) >> 13)                \
       : (from) - ((AbsAngleDelta12((to), (from)) * (t)) >> 13))

/* 0x800314b4 (0x1024): the save prompt a save host (type 110) opens.
   Mode 0 swings the host, Sparx and the camera from their start to their
   end keyframes over 0x40 frames (easing on a sine), then mode 1 runs the
   "save game?" menu: the cursor (0 save, 1 replay the rescue of the
   host's dragon, 2 leave), the three memory card stages (load the
   directory, write the save, read it back and verify the checksum) and the
   result screens, ending in EndSaveMenuToWorld. */
void SaveMenu_Update(void) {
  int vec[3];
  short ang[3];
  int t;
  int a;
  int d;

  SM[1] += g_nFrameStep;
  RotateLightVectorXZ(3);
  g_apSaveMenuAnimActorListBlock[0] = ACT;
  g_apSaveMenuAnimActorListBlock[1] = 0;
  TickActorAnims(g_apSaveMenuAnimActorListBlock, 0);
  switch (SM[0]) {
  case 0:
    if (SM[1] < 0x40) {
      unsigned char *actor;

      t = LookupSine((SM[1] << 5) - 0x400) + 0x1000;
      actor =
          (unsigned char *)g_pActorListBase + ((int *)ACT[0])[1] * ACTOR_SIZE;
      if (ACTOR_RAW(actor, unsigned char, mode) == 2) {
        ACTOR_RAW(actor, unsigned char, subMode) += g_nFrameStep;
        if (ACTOR_RAW(actor, unsigned char, subMode) >= 0x30) {
          ACTOR_RAW(actor, unsigned char, mode) = 0;
          ACTOR_RAW(actor, unsigned char, anim) = 0;
        }
      }
      a = LERP_ANGLE(SM[14], SM[11], t);
      d = g_nSaveMenuHostFromDist +
          (((g_nSaveMenuHostToDist - g_nSaveMenuHostFromDist) * t) >> 13);
      FLD(ACT, 0xc) = g_anSpyroWorldPos[0] + ((LookupCosine(a) * d) >> 12);
      FLD(ACT, 0x10) = g_nSpyroWorldPosY + ((LookupSine(a) * d) >> 12);
      FLD(ACT, 0x14) =
          g_nSaveMenuHostFromZ +
          (((g_nSaveMenuHostToZ - g_nSaveMenuHostFromZ) * t) >> 13);
      TrackEntityYawTowardSpyro(ACT, 3, 0, 0);
      FindGroundHeightBelow(ACT + 3, 0x1000);
      EncodeCachedVecToActorDirCode(ACT);
      if (g_pSparxActor != 0) {
        a = LERP_ANGLE(g_nSaveMenuSparxToAngle, g_nSaveMenuSparxFromAngle, t);
        d = g_nSaveMenuSparxFromDist +
            (((g_nSaveMenuSparxToDist - g_nSaveMenuSparxFromDist) * t) >> 13);
        ((int *)g_pSparxActor)[3] =
            g_anSpyroWorldPos[0] + ((LookupCosine(a) * d) >> 12);
        ((int *)g_pSparxActor)[4] =
            g_nSpyroWorldPosY + ((LookupSine(a) * d) >> 12);
      }
      a = LERP_ANGLE(SM[24], SM[21], t);
      d = g_nSaveMenuCameraFromDist +
          (((g_nSaveMenuCameraToDist - g_nSaveMenuCameraFromDist) * t) >> 13);
      g_anCameraPos[0] = g_anSpyroWorldPos[0] + ((LookupCosine(a) * d) >> 12);
      g_anCameraPos[1] = g_nSpyroWorldPosY + ((LookupSine(a) * d) >> 12);
      g_anCameraPos[2] =
          g_nSaveMenuCameraFromZ +
          (((g_nSaveMenuCameraToZ - g_nSaveMenuCameraFromZ) * t) >> 13);
      AddVector(vec, g_anSpyroWorldPos, ACT + 3);
      vec[2] += 0x240;
      RShiftVector3(vec, 1);
      SubtractVector(vec, vec, g_anCameraPos);
      if (g_nSaveMenuAnimPhase < 0x20) {
        ang[0] = 0;
        ang[1] = ArcTan2(VectorLength(vec, 0), -vec[2], 1);
        ang[2] = ArcTan2(vec[0], vec[1], 1);
        g_nCameraEulerRoll = (g_nCameraEulerRoll +
                              ((SignedAngleDelta12(ang[0], g_nCameraEulerRoll) *
                                g_nSaveMenuAnimPhase) >>
                               5)) &
                             0xFFF;
        g_nCameraEulerPitch =
            (g_nCameraEulerPitch +
             ((SignedAngleDelta12(ang[1], g_nCameraEulerPitch) *
               g_nSaveMenuAnimPhase) >>
              5)) &
            0xFFF;
        g_nCameraEulerYaw = (g_nCameraEulerYaw +
                             ((SignedAngleDelta12(ang[2], g_nCameraEulerYaw) *
                               g_nSaveMenuAnimPhase) >>
                              5)) &
                            0xFFF;
      } else {
        g_nCameraEulerRoll = 0;
        g_nCameraEulerPitch = ArcTan2(VectorLength(vec, 0), -vec[2], 1);
        g_nCameraEulerYaw = ArcTan2(vec[0], vec[1], 1);
      }
      if (((SM[28] - SM[27]) & 0xFFF) < 0x801) {
        g_abSpyroPersistentEuler[2] =
            (SM[27] + ((AbsAngleDelta12(SM[28], SM[27]) * t) >> 13)) >> 4;
      } else {
        g_abSpyroPersistentEuler[2] =
            (SM[27] - ((AbsAngleDelta12(SM[28], SM[27]) * t) >> 13)) >> 4;
      }
      ApplyEulerRotation(SPYB + SPYB_EULER, (int *)(SPYB + SPYB_BODY_MTX), 0);
      g_bSpyroSwingByteEuler1 = (unsigned int)t >> 9;
      AdvanceSpyroAnimFrame(0x10);
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
      TickSpyroAttackEffects(g_nFrameStep);
      ((void (*)(int))g_pfnLevelAdvanceEmitList)(g_nFrameStep);
    } else {
      FLD(ACT, 0xc) =
          g_anSpyroWorldPos[0] +
          ((LookupCosine(g_nSaveMenuHostToAngle) * g_nSaveMenuHostToDist) >>
           12);
      FLD(ACT, 0x10) =
          g_anSpyroWorldPos[1] +
          ((LookupSine(g_nSaveMenuHostToAngle) * g_nSaveMenuHostToDist) >> 12);
      FLD(ACT, 0x14) = g_nSaveMenuHostToZ;
      {
        int *act = ACT;

        ((unsigned char *)ACT)[0x46] = ArcTan2(
            g_anSpyroWorldPos[0] - act[3], g_anSpyroWorldPos[1] - act[4], 0);
      }
      if (g_pSparxActor != 0) {
        ((int *)g_pSparxActor)[3] =
            g_anSpyroWorldPos[0] + ((LookupCosine(SM[19]) * SM[20]) >> 12);
        ((int *)g_pSparxActor)[4] =
            g_anSpyroWorldPos[1] + ((LookupSine(SM[19]) * SM[20]) >> 12);
      }
      g_anCameraPos[0] =
          g_anSpyroWorldPos[0] +
          ((LookupCosine(g_nSaveMenuCameraToAngle) * g_nSaveMenuCameraToDist) >>
           12);
      {
        int sn = LookupSine(g_nSaveMenuCameraToAngle);

        SPYB[SPYB_SUB_FRAME_TIMER] = 4;
        SPYB[SPYB_ANIM_CURRENT] = 0;
        SPYB[SPYB_ANIM_FRAME] = 0;
        g_anSpyroWorldPosBlock[0x16] = 4;
        SM[0] = 1;
        SM[1] = 0;
        g_anCameraPos[2] = g_nSaveMenuCameraToZ;
        SPYB[SPYB_EULER_Z] = g_nSaveMenuSpyroToYaw >> 4;
        g_anCameraPos[1] =
            g_anSpyroWorldPos[1] + ((sn * g_nSaveMenuCameraToDist) >> 12);
      }
    }
    break;
  case 1:
    AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2();
    g_dwGamestateFrames++;
    ((void (*)(int))g_pfnLevelAdvanceEmitList)(g_nFrameStep);
    switch (SM[3]) {
    case 0:
      if (g_dwPadPressed & PAD_DOWN) {
        PlaySoundEffect(LEVEL_SFX(0x2D), 0, 0x10, 0);
        g_nSaveMenuAnimPhase = 0;
        if (++g_nSaveMenuCursor >= 3) {
          g_nSaveMenuCursor = 0;
        }
      } else if (g_dwPadPressed & PAD_UP) {
        PlaySoundEffect(LEVEL_SFX(0x2D), 0, 0x10, 0);
        g_nSaveMenuAnimPhase = 0;
        if (--g_nSaveMenuCursor < 0) {
          g_nSaveMenuCursor = 2;
        }
      }
      if (SM[1] >= 8 && (g_dwPadPressed & PAD_CROSS)) {
        PlaySoundEffect(LEVEL_SFX(0x2E), 0, 0x10, 0);
        if (g_nSaveMenuCursor == 0) {
          SM[1] = 0;
          if (g_nSaveSlotLoaded != 0) {
            g_nSaveMenuSubstate = 2;
          } else {
            g_nSaveMenuSubstate = 1;
          }
        } else if (g_nSaveMenuCursor == 1) {
          unsigned char *actor;
          int *path;

          actor =
              (unsigned char *)g_pActorListBase + *(int *)ACT[0] * ACTOR_SIZE;
          path = *(int **)actor;
          if (path[9] == -1 && path[6] != -1) {
            EndSaveMenuToWorld();
            ACTOR_RAW(actor, unsigned char, mode) = 2;
            ((int *)actor)[2] = 0;
            ((short *)actor)[0x1A] = -1;
            BeginDragonRescue(actor);
            g_nWorldDragonsRescued--;
            g_anLevelDragonsRescued[g_nLevelIndex]--;
          }
        } else {
          EndSaveMenuToWorld();
          PlaySoundEffect(LEVEL_SFX(0x2E), 0, 0x10, 0);
        }
      }
      break;
    case 1:
      if (SM[1] >= 0x3C && (g_dwPadPressed & PAD_CROSS)) {
        PlaySoundEffect(LEVEL_SFX(0x2E), 0, 0x10, 0);
        SM[1] = 0;
        g_nSaveMenuSubstate = 0;
        g_nSaveMenuCursor = 2;
      }
      break;
    case 2: {
      int *cur = &SM[2];

      if (*cur == 0) {
        MemCardOpenSession();
        MemCardLoad(g_nSaveSlotSelected);
        *cur = 1;
        break;
      }
      if (*cur == 1) {
        int st;

        if (MemCardSync(1, &SM[9], &SM[10]) == 0) {
          break;
        }
        st = SM[10];
        if (st == 0 || st == 3) {
          MemCardRead(g_nSaveSlotSelected, g_szMemCardSaveName,
                      (char *)g_pRenderScratchPrimTop - 0x600, 0, 0x80);
          *cur = 2;
          break;
        }
        if (st == 4) {
          g_nSaveMenuSubstate = st;
        } else {
          g_nSaveMenuSubstate = 3;
        }
      } else if (*cur == 2) {
        int st;

        if (MemCardSync(1, &SM[9], &SM[10]) == 0) {
          break;
        }
        st = SM[10];
        if (st == 0) {
          if (g_nSaveFileTag ==
              *(unsigned short *)((char *)g_pRenderScratchPrimTop - 0x582)) {
            BuildSaveGameBuffer((char *)g_apRenderScratchPrimTopBlock[0] -
                                0x600);
            MemCardWrite(g_nSaveSlotSelected, g_szMemCardSaveName,
                         (char *)g_apRenderScratchPrimTopBlock[0] - 0x600,
                         g_nSaveSlotIndex * 0x600 + 0x200, 0x600);
            *cur = 3;
            break;
          }
          g_nSaveMenuSubstate = 4;
        } else if (st == 5) {
          g_nSaveMenuSubstate = 4;
        } else {
          g_nSaveMenuSubstate = 5;
        }
      } else if (*cur == 3) {
        if (MemCardSync(1, &SM[9], &SM[10]) == 0) {
          break;
        }
        if (SM[10] == 0) {
          MemCardRead(g_nSaveSlotSelected, g_szMemCardSaveName,
                      (char *)g_pRenderScratchPrimTop - 0x600,
                      g_nSaveSlotIndex * 0x600 + 0x200, 0x600);
          *cur = 4;
          break;
        }
        g_nSaveMenuSubstate = 5;
      } else {
        int v;

        if (MemCardSync(1, &SM[9], &SM[10]) == 0) {
          break;
        }
        if (SM[10] == 0 &&
            ComputeSaveGameChecksum((unsigned char *)g_pRenderScratchPrimTop -
                                    0x600) ==
                *(int *)((char *)g_pRenderScratchPrimTop - 0x74)) {
          v = 7;
        } else {
          v = 5;
        }
        g_nSaveMenuSubstate = v;
      }
      g_nSaveMenuAnimPhase = 0;
    } break;
    case 3:
    case 4:
    case 5:
      if (SM[1] >= 0x3C && (g_dwPadPressed & PAD_CROSS)) {
        PlaySoundEffect(LEVEL_SFX(0x2E), 0, 0x10, 0);
        SM[1] = 0;
        g_nSaveMenuSubstate = 6;
        g_nSaveMenuCursor = 0;
      }
      break;
    case 6:
      if (g_adwPadPressedBlock[0] & (PAD_UP | PAD_DOWN)) {
        PlaySoundEffect(LEVEL_SFX(0x2D), 0, 0x10, 0);
        SM[2] = 1 - SM[2];
      }
      if (SM[1] >= 0x20 && (g_adwPadPressedBlock[0] & PAD_CROSS)) {
        MemCardCloseSession();
        if (g_nSaveMenuCursor == 0) {
          PlaySoundEffect(LEVEL_SFX(0x2E), 0, 0x10, 0);
          SM[1] = 0;
          g_nSaveMenuSubstate = 2;
          g_nSaveMenuCursor = 0;
        } else {
          EndSaveMenuToWorld();
          PlaySoundEffect(LEVEL_SFX(0x2E), 0, 0x10, 0);
        }
      }
      break;
    case 7:
      if (g_nSaveMenuAnimPhase >= 8 && (g_dwPadPressed & PAD_CROSS)) {
        MemCardCloseSession();
        EndSaveMenuToWorld();
        PlaySoundEffect(LEVEL_SFX(0x2E), 0, 0x10, 0);
      }
      break;
    }
    break;
  }
}
