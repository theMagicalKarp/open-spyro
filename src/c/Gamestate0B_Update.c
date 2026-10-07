#include "globals.h"

extern void RotateLightVectorXZ(int steps);
extern void TickGemCutsceneActorAnims(void **actors, int mode);
extern int LookupSine(int angle);
extern int LookupCosine(int angle);
extern int AbsAngleDelta12(int a, int b);
extern int SignedAngleDelta12(int a, int b);
extern void TrackEntityYawTowardSpyro(void *actor, int step, int deadzone,
                                      int snap);
extern int FindGroundHeightBelow(int *pos, int depth);
extern void EncodeCachedVecToActorDirCode(void *actor);
extern void AddVector(int *dst, int *a, int *b);
extern void SubtractVector(int *dst, int *a, int *b);
extern void RShiftVector3(int *vec, int shift);
extern int VectorLength(int *vec, int include_z);
extern int ArcTan2(int y, int x, int high_precision);
extern void ApplyEulerRotation(unsigned char *euler, int *mtx, int *scale);
extern void AdvanceSpyroAnimFrame(int delta);
extern void TickSpyroAnimLayer1(void);
extern void TickSpyroAnimLayer2(void);
extern void TickSpyroAttackEffects(int steps);
extern int PlaySoundEffect(unsigned int sample, int owner, unsigned int mode,
                           void *marker);
extern void EndSaveMenuToWorld(void);
extern void BeginGemPickupOverlay(void *actor);
extern void MemCardOpenSession(void);
extern void MemCardCloseSession(void);
extern void MemCardLoad(int slot);
extern int MemCardSync(int mode, int *cmd, int *result);
extern void MemCardRead(int slot, char *name, void *buf, int offset, int size);
extern void MemCardWrite(int slot, char *name, void *buf, int offset, int size);
extern void BuildSaveGameBuffer(void *buf);
extern int ComputeSaveGameChecksum(unsigned char *data);

/* The save-menu / dragon-cutscene record at 0x80078d00: [0] mode, [1] phase,
   [2] cursor, [3] substate, [9]/[10] memcard sync results, then from/to
   keyframes for the dragon (+0x2C/+0x38), the overlay record (+0x44/+0x4C),
   the camera (+0x54/+0x60) and Spyro's yaw (+0x6C/+0x70). */
extern int g_anSaveMenuBlock[];
extern int g_anSpyroWorldPosBlock[];
extern unsigned int g_adwPadPressedBlock[];
extern void *g_apSaveMenuAnimActorListBlock[];
extern void *g_apRenderScratchPrimTopBlock[];
extern int g_nSpyroWorldPosY; /* 0x80078a5c — g_anSpyroWorldPos[1] */
extern unsigned char g_bSpyroSwingByteEuler1; /* 0x80078a69 */
extern char D_80010DB8[];                     /* memory card save file name */
extern int D_80078D14;
extern int D_80078D1C;
extern int D_80078D20;
extern int D_80078D30;
extern int D_80078D34;
extern int D_80078D38;
extern int D_80078D3C;
extern int D_80078D40;
extern int D_80078D44;
extern int D_80078D48;
extern int D_80078D4C;
extern int D_80078D50;
extern int D_80078D58;
extern int D_80078D5C;
extern int D_80078D60;
extern int D_80078D64;
extern int D_80078D68;
extern int D_80078D70;
extern int *volatile D_80078D74; /* the rescued dragon's actor */

#define SM g_anSaveMenuBlock
#define SPYB ((unsigned char *)g_anSpyroWorldPosBlock)
#define ACT D_80078D74
#define FLD(p, off) (*(int *)((char *)(p) + (off)))
#define SAMPLE(i) (((unsigned char *)g_pLevelSampleBankHeader)[i])

/* Eases an angle from `from` toward `to` by t/0x2000 along the short arc. */
#define LERP_ANGLE(to, from, t)                                                \
  ((((to) - (from)) & 0xFFF) < 0x801                                           \
       ? (from) + ((AbsAngleDelta12((to), (from)) * (t)) >> 13)                \
       : (from) - ((AbsAngleDelta12((to), (from)) * (t)) >> 13))

/* 0x800314b4 (0x1024): the dragon-rescue save prompt. Mode 0 swings the
   rescued dragon, the overlay record and the camera from their start to their
   end keyframes over 0x40 frames (easing on a sine), then mode 1 runs the
   "save game?" menu: the cursor, the three memory card stages (load the
   directory, write the save, read it back and verify the checksum) and the
   result screens, ending in EndSaveMenuToWorld. */
void Gamestate0B_Update(void) {
  int vec[3];
  short ang[3];
  int t;
  int a;
  int d;

  SM[1] += g_nFrameStep;
  RotateLightVectorXZ(3);
  g_apSaveMenuAnimActorListBlock[0] = ACT;
  g_apSaveMenuAnimActorListBlock[1] = 0;
  TickGemCutsceneActorAnims(g_apSaveMenuAnimActorListBlock, 0);
  switch (SM[0]) {
  case 0:
    if (SM[1] < 0x40) {
      unsigned char *actor;

      t = LookupSine((SM[1] << 5) - 0x400) + 0x1000;
      actor = (unsigned char *)g_pActorListBase + ((int *)ACT[0])[1] * 0x58;
      if (actor[0x48] == 2) {
        actor[0x49] += g_nFrameStep;
        if (actor[0x49] >= 0x30) {
          actor[0x48] = 0;
          actor[0x3C] = 0;
        }
      }
      a = LERP_ANGLE(SM[14], SM[11], t);
      d = D_80078D30 + (((D_80078D3C - D_80078D30) * t) >> 13);
      FLD(ACT, 0xc) = g_anSpyroWorldPos[0] + ((LookupCosine(a) * d) >> 12);
      FLD(ACT, 0x10) = g_nSpyroWorldPosY + ((LookupSine(a) * d) >> 12);
      FLD(ACT, 0x14) = D_80078D34 + (((D_80078D40 - D_80078D34) * t) >> 13);
      TrackEntityYawTowardSpyro(ACT, 3, 0, 0);
      FindGroundHeightBelow(ACT + 3, 0x1000);
      EncodeCachedVecToActorDirCode(ACT);
      if (g_pLevelOverlayInitRecord != 0) {
        a = LERP_ANGLE(D_80078D4C, D_80078D44, t);
        d = D_80078D48 + (((D_80078D50 - D_80078D48) * t) >> 13);
        ((int *)g_pLevelOverlayInitRecord)[3] =
            g_anSpyroWorldPos[0] + ((LookupCosine(a) * d) >> 12);
        ((int *)g_pLevelOverlayInitRecord)[4] =
            g_nSpyroWorldPosY + ((LookupSine(a) * d) >> 12);
      }
      a = LERP_ANGLE(SM[24], SM[21], t);
      d = D_80078D58 + (((D_80078D64 - D_80078D58) * t) >> 13);
      g_anCameraPos[0] = g_anSpyroWorldPos[0] + ((LookupCosine(a) * d) >> 12);
      g_anCameraPos[1] = g_nSpyroWorldPosY + ((LookupSine(a) * d) >> 12);
      g_anCameraPos[2] = D_80078D5C + (((D_80078D68 - D_80078D5C) * t) >> 13);
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
      ApplyEulerRotation(SPYB + 0xC, (int *)(SPYB + 0x34), 0);
      g_bSpyroSwingByteEuler1 = (unsigned int)t >> 9;
      AdvanceSpyroAnimFrame(0x10);
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
      TickSpyroAttackEffects(g_nFrameStep);
      ((void (*)(int))g_pfnGamestate0LateHook)(g_nFrameStep);
    } else {
      FLD(ACT, 0xc) = g_anSpyroWorldPos[0] +
                      ((LookupCosine(D_80078D38) * D_80078D3C) >> 12);
      FLD(ACT, 0x10) =
          g_anSpyroWorldPos[1] + ((LookupSine(D_80078D38) * D_80078D3C) >> 12);
      FLD(ACT, 0x14) = D_80078D40;
      {
        int *act = ACT;

        ((unsigned char *)ACT)[0x46] = ArcTan2(
            g_anSpyroWorldPos[0] - act[3], g_anSpyroWorldPos[1] - act[4], 0);
      }
      if (g_pLevelOverlayInitRecord != 0) {
        ((int *)g_pLevelOverlayInitRecord)[3] =
            g_anSpyroWorldPos[0] + ((LookupCosine(SM[19]) * SM[20]) >> 12);
        ((int *)g_pLevelOverlayInitRecord)[4] =
            g_anSpyroWorldPos[1] + ((LookupSine(SM[19]) * SM[20]) >> 12);
      }
      g_anCameraPos[0] = g_anSpyroWorldPos[0] +
                         ((LookupCosine(D_80078D60) * D_80078D64) >> 12);
      {
        int sn = LookupSine(D_80078D60);

        SPYB[0x24] = 4;
        SPYB[0x19] = 0;
        SPYB[0x1F] = 0;
        g_anSpyroWorldPosBlock[0x16] = 4;
        SM[0] = 1;
        SM[1] = 0;
        g_anCameraPos[2] = D_80078D68;
        SPYB[0xE] = D_80078D70 >> 4;
        g_anCameraPos[1] = g_anSpyroWorldPos[1] + ((sn * D_80078D64) >> 12);
      }
    }
    break;
  case 1:
    AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2();
    g_dwGamestateFrames++;
    ((void (*)(int))g_pfnGamestate0LateHook)(g_nFrameStep);
    switch (SM[3]) {
    case 0:
      if (g_dwPadPressed & 0x4000) {
        PlaySoundEffect(SAMPLE(0x2D), 0, 0x10, 0);
        g_nSaveMenuAnimPhase = 0;
        if (++g_nSaveMenuCursor >= 3) {
          g_nSaveMenuCursor = 0;
        }
      } else if (g_dwPadPressed & 0x1000) {
        PlaySoundEffect(SAMPLE(0x2D), 0, 0x10, 0);
        g_nSaveMenuAnimPhase = 0;
        if (--g_nSaveMenuCursor < 0) {
          g_nSaveMenuCursor = 2;
        }
      }
      if (SM[1] >= 8 && (g_dwPadPressed & 0x40)) {
        PlaySoundEffect(SAMPLE(0x2E), 0, 0x10, 0);
        if (g_nSaveMenuCursor == 0) {
          SM[1] = 0;
          if (D_80078D14 != 0) {
            g_nSaveMenuSubstate = 2;
          } else {
            g_nSaveMenuSubstate = 1;
          }
        } else if (g_nSaveMenuCursor == 1) {
          unsigned char *actor;
          int *path;

          actor = (unsigned char *)g_pActorListBase + *(int *)ACT[0] * 0x58;
          path = *(int **)actor;
          if (path[9] == -1 && path[6] != -1) {
            EndSaveMenuToWorld();
            actor[0x48] = 2;
            ((int *)actor)[2] = 0;
            ((short *)actor)[0x1A] = -1;
            BeginGemPickupOverlay(actor);
            g_nWorldDragonsRescued--;
            g_anLevelDragonsRescued[g_nLevelIntroIndex]--;
          }
        } else {
          EndSaveMenuToWorld();
          PlaySoundEffect(SAMPLE(0x2E), 0, 0x10, 0);
        }
      }
      break;
    case 1:
      if (SM[1] >= 0x3C && (g_dwPadPressed & 0x40)) {
        PlaySoundEffect(SAMPLE(0x2E), 0, 0x10, 0);
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
          MemCardRead(g_nSaveSlotSelected, D_80010DB8,
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
          if (D_80078D20 ==
              *(unsigned short *)((char *)g_pRenderScratchPrimTop - 0x582)) {
            BuildSaveGameBuffer((char *)g_apRenderScratchPrimTopBlock[0] -
                                0x600);
            MemCardWrite(g_nSaveSlotSelected, D_80010DB8,
                         (char *)g_apRenderScratchPrimTopBlock[0] - 0x600,
                         D_80078D1C * 0x600 + 0x200, 0x600);
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
          MemCardRead(g_nSaveSlotSelected, D_80010DB8,
                      (char *)g_pRenderScratchPrimTop - 0x600,
                      D_80078D1C * 0x600 + 0x200, 0x600);
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
      if (SM[1] >= 0x3C && (g_dwPadPressed & 0x40)) {
        PlaySoundEffect(SAMPLE(0x2E), 0, 0x10, 0);
        SM[1] = 0;
        g_nSaveMenuSubstate = 6;
        g_nSaveMenuCursor = 0;
      }
      break;
    case 6:
      if (g_adwPadPressedBlock[0] & 0x5000) {
        PlaySoundEffect(SAMPLE(0x2D), 0, 0x10, 0);
        SM[2] = 1 - SM[2];
      }
      if (SM[1] >= 0x20 && (g_adwPadPressedBlock[0] & 0x40)) {
        MemCardCloseSession();
        if (g_nSaveMenuCursor == 0) {
          PlaySoundEffect(SAMPLE(0x2E), 0, 0x10, 0);
          SM[1] = 0;
          g_nSaveMenuSubstate = 2;
          g_nSaveMenuCursor = 0;
        } else {
          EndSaveMenuToWorld();
          PlaySoundEffect(SAMPLE(0x2E), 0, 0x10, 0);
        }
      }
      break;
    case 7:
      if (g_nSaveMenuAnimPhase >= 8 && (g_dwPadPressed & 0x40)) {
        MemCardCloseSession();
        EndSaveMenuToWorld();
        PlaySoundEffect(SAMPLE(0x2E), 0, 0x10, 0);
      }
      break;
    }
    break;
  }
}
