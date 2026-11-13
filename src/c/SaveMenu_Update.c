#include "actor_states.h"
#include "funcs.h"
#include "globals.h"
#include "level_sfx.h"
#include "main_shared.h"
#include "pad_buttons.h"
#include "sample_bank.h"
#include "save_game.h"
#include "save_menu.h"
#include "spyro_block.h"
#include "vecmath.h"

/* SM indexes the save-menu record by its SaveMenuWord (save_menu.h). */
#define SM g_anSaveMenuBlock
#define SPYB ((unsigned char *)g_anSpyroWorldPosBlock)
#define ACT g_pSaveMenuHostActor
/* The host position stores go through ACTOR_RAW: as Actor fields they
   reschedule (A400). */

/* Eases an angle from `from` toward `to` by t/0x2000 along the short arc. */
#define LERP_ANGLE(to, from, t)                                                \
  (ANGLE12_SHORT_WAY_UP((to), (from))                                          \
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

  SM[SM_PHASE] += g_nFrameStep;
  RotateLightVectorXZ(3);
  g_apActiveActorListBlock[0] = ACT;
  g_apActiveActorListBlock[1] = 0;
  TickActorAnims(g_apActiveActorListBlock, ANIM_TICK_NORMAL);
  switch (SM[SM_MODE]) {
  case SAVE_MODE_SWING_IN:
    if (SM[SM_PHASE] < 0x40) {
      Actor *actor;

      t = SINE_EASE(SM[SM_PHASE] << 5);
      actor = &g_pActorListBase[((SaveHostState *)ACT->state)->anchor];
      if (actor->mode == CHECKPT_FLASH) {
        actor->subMode += g_nFrameStep;
        if (actor->subMode >= CHECKPT_FLASH_FRAMES) {
          actor->mode = CHECKPT_REARM;
          actor->anim = CHECKPT_ANIM_REST;
        }
      }
      a = LERP_ANGLE(SM[SM_HOST_TO_ANGLE], SM[SM_HOST_FROM_ANGLE], t);
      d = g_nSaveMenuHostFromDist +
          (((g_nSaveMenuHostToDist - g_nSaveMenuHostFromDist) * t) >> 13);
      ACTOR_RAW(ACT, int, posX) =
          g_anSpyroWorldPos[0] + ((LookupCosine(a) * d) >> 12);
      ACTOR_RAW(ACT, int, posY) =
          g_nSpyroWorldPosY + ((LookupSine(a) * d) >> 12);
      ACTOR_RAW(ACT, int, posZ) =
          g_nSaveMenuHostFromZ +
          (((g_nSaveMenuHostToZ - g_nSaveMenuHostFromZ) * t) >> 13);
      TrackEntityYawTowardSpyro(ACT, 3, 0, YAW_HOLD_IN_BAND);
      FindGroundHeightBelow(&ACT->posX, 0x1000);
      EncodeHitNormalToActorGroundCode(ACT);
      if (g_pSparxActor != 0) {
        a = LERP_ANGLE(g_nSaveMenuSparxToAngle, g_nSaveMenuSparxFromAngle, t);
        d = g_nSaveMenuSparxFromDist +
            (((g_nSaveMenuSparxToDist - g_nSaveMenuSparxFromDist) * t) >> 13);
        g_pSparxActor->posX =
            g_anSpyroWorldPos[0] + ((LookupCosine(a) * d) >> 12);
        g_pSparxActor->posY = g_nSpyroWorldPosY + ((LookupSine(a) * d) >> 12);
      }
      a = LERP_ANGLE(SM[SM_CAMERA_TO_ANGLE], SM[SM_CAMERA_FROM_ANGLE], t);
      d = g_nSaveMenuCameraFromDist +
          (((g_nSaveMenuCameraToDist - g_nSaveMenuCameraFromDist) * t) >> 13);
      g_anCameraPos[0] = g_anSpyroWorldPos[0] + ((LookupCosine(a) * d) >> 12);
      g_anCameraPos[1] = g_nSpyroWorldPosY + ((LookupSine(a) * d) >> 12);
      g_anCameraPos[2] =
          g_nSaveMenuCameraFromZ +
          (((g_nSaveMenuCameraToZ - g_nSaveMenuCameraFromZ) * t) >> 13);
      AddVector(vec, g_anSpyroWorldPos, &ACT->posX);
      vec[2] += 0x240;
      RShiftVector3(vec, 1);
      SubtractVector(vec, vec, g_anCameraPos);
      if (g_nSaveMenuAnimPhase < 0x20) {
        ang[0] = 0;
        ang[1] = ArcTan2(VectorLength(vec, VEC_LEN_XY), -vec[2], ATAN_ANGLE12);
        ang[2] = ArcTan2(vec[0], vec[1], ATAN_ANGLE12);
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
        g_nCameraEulerPitch =
            ArcTan2(VectorLength(vec, VEC_LEN_XY), -vec[2], ATAN_ANGLE12);
        g_nCameraEulerYaw = ArcTan2(vec[0], vec[1], ATAN_ANGLE12);
      }
      if (ANGLE12_SHORT_WAY_UP(SM[SM_SPYRO_TO_YAW], SM[SM_SPYRO_FROM_YAW])) {
        g_abSpyroPersistentEuler[2] =
            (SM[SM_SPYRO_FROM_YAW] +
             ((AbsAngleDelta12(SM[SM_SPYRO_TO_YAW], SM[SM_SPYRO_FROM_YAW]) *
               t) >>
              13)) >>
            4;
      } else {
        g_abSpyroPersistentEuler[2] =
            (SM[SM_SPYRO_FROM_YAW] -
             ((AbsAngleDelta12(SM[SM_SPYRO_TO_YAW], SM[SM_SPYRO_FROM_YAW]) *
               t) >>
              13)) >>
            4;
      }
      ApplyEulerRotation(SPYB + SPYB_EULER, (int *)(SPYB + SPYB_BODY_MTX), 0);
      g_bSpyroSwingByteEuler1 = (unsigned int)t >> 9;
      AdvanceSpyroAnimFrame(0x10);
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
      TickSpyroAttackEffects(g_nFrameStep);
      g_pfnLevelAdvanceEmitList(g_nFrameStep);
    } else {
      ACTOR_RAW(ACT, int, posX) =
          g_anSpyroWorldPos[0] +
          ((LookupCosine(g_nSaveMenuHostToAngle) * g_nSaveMenuHostToDist) >>
           12);
      ACTOR_RAW(ACT, int, posY) =
          g_anSpyroWorldPos[1] +
          ((LookupSine(g_nSaveMenuHostToAngle) * g_nSaveMenuHostToDist) >> 12);
      ACTOR_RAW(ACT, int, posZ) = g_nSaveMenuHostToZ;
      {
        Actor *act = ACT;

        ACT->rotZ = ArcTan2(g_anSpyroWorldPos[0] - act->posX,
                            g_anSpyroWorldPos[1] - act->posY, ATAN_ANGLE8);
      }
      if (g_pSparxActor != 0) {
        g_pSparxActor->posX =
            g_anSpyroWorldPos[0] +
            ((LookupCosine(SM[SM_SPARX_TO_ANGLE]) * SM[SM_SPARX_TO_DIST]) >>
             12);
        g_pSparxActor->posY =
            g_anSpyroWorldPos[1] +
            ((LookupSine(SM[SM_SPARX_TO_ANGLE]) * SM[SM_SPARX_TO_DIST]) >> 12);
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
        g_anSpyroWorldPosBlock[SPY_ANIM_PLAY_RATE] = 4;
        SM[SM_MODE] = SAVE_MODE_MENU;
        SM[SM_PHASE] = 0;
        g_anCameraPos[2] = g_nSaveMenuCameraToZ;
        SPYB[SPYB_EULER_Z] = g_nSaveMenuSpyroToYaw >> 4;
        g_anCameraPos[1] =
            g_anSpyroWorldPos[1] + ((sn * g_nSaveMenuCameraToDist) >> 12);
      }
    }
    break;
  case SAVE_MODE_MENU:
    AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2();
    g_dwGamestateFrames++;
    g_pfnLevelAdvanceEmitList(g_nFrameStep);
    switch (SM[SM_SUBSTATE]) {
    case SAVE_SCREEN_MAIN:
      if (g_dwPadPressed & PAD_DOWN) {
        PlaySoundEffect(LEVEL_SFX(SFX_MENU_MOVE), 0, SFX_MODE_PANNED, 0);
        g_nSaveMenuAnimPhase = 0;
        if (++g_nSaveMenuCursor >= SAVE_ROW_COUNT) {
          g_nSaveMenuCursor = SAVE_ROW_SAVE;
        }
      } else if (g_dwPadPressed & PAD_UP) {
        PlaySoundEffect(LEVEL_SFX(SFX_MENU_MOVE), 0, SFX_MODE_PANNED, 0);
        g_nSaveMenuAnimPhase = 0;
        if (--g_nSaveMenuCursor < 0) {
          g_nSaveMenuCursor = SAVE_ROW_CONTINUE;
        }
      }
      if (SM[SM_PHASE] >= 8 && (g_dwPadPressed & PAD_CROSS)) {
        PlaySoundEffect(LEVEL_SFX(SFX_MENU_SELECT), 0, SFX_MODE_PANNED, 0);
        if (g_nSaveMenuCursor == SAVE_ROW_SAVE) {
          SM[SM_PHASE] = 0;
          if (g_nSaveSlotLoaded != 0) {
            g_nSaveMenuSubstate = SAVE_SCREEN_SAVING;
          } else {
            g_nSaveMenuSubstate = SAVE_SCREEN_NO_SESSION;
          }
        } else if (g_nSaveMenuCursor == SAVE_ROW_REPLAY_DRAGON) {
          Actor *actor;
          DragonStatueState *statue;

          actor = &g_pActorListBase[((SaveHostState *)ACT->state)->armer];
          statue = actor->state;
          if (statue->reward == -1 && statue->cutsceneId != -1) {
            EndSaveMenuToWorld();
            actor->mode = STATUE_RESCUING;
            actor->collision = 0;
            actor->gridCell = -1;
            BeginDragonRescue(actor);
            g_nTotalDragonsRescued--;
            g_anLevelDragonsRescued[g_nLevelIndex]--;
          }
        } else {
          EndSaveMenuToWorld();
          PlaySoundEffect(LEVEL_SFX(SFX_MENU_SELECT), 0, SFX_MODE_PANNED, 0);
        }
      }
      break;
    case SAVE_SCREEN_NO_SESSION:
      if (SM[SM_PHASE] >= 0x3C && (g_dwPadPressed & PAD_CROSS)) {
        PlaySoundEffect(LEVEL_SFX(SFX_MENU_SELECT), 0, SFX_MODE_PANNED, 0);
        SM[SM_PHASE] = 0;
        g_nSaveMenuSubstate = SAVE_SCREEN_MAIN;
        g_nSaveMenuCursor = SAVE_ROW_CONTINUE;
      }
      break;
    case SAVE_SCREEN_SAVING: {
      int *cur = &SM[SM_CURSOR];

      if (*cur == SAVE_STEP_OPEN) {
        MemCardOpenSession();
        MemCardLoad(g_nSaveSlotSelected);
        *cur = SAVE_STEP_READ_HEADER;
        break;
      }
      if (*cur == SAVE_STEP_READ_HEADER) {
        int st;

        if (MemCardSync(1, &SM[SM_SYNC_CMD], &SM[SM_SYNC_RESULT]) == 0) {
          break;
        }
        st = SM[SM_SYNC_RESULT];
        if (st == McErrNone || st == McErrNewCard) {
          MemCardRead(g_nSaveSlotSelected, g_szMemCardSaveName,
                      (char *)g_pRenderScratchPrimTop - SAVE_IMAGE_BYTES, 0,
                      (int)&((McFileHeader *)0)->icon); /* through clut[] */
          *cur = SAVE_STEP_WRITE;
          break;
        }
        if (st == McErrNotFormat) {
          /* McErrNotFormat == SAVE_SCREEN_NO_FILE: the original stores st */
          g_nSaveMenuSubstate = st;
        } else {
          g_nSaveMenuSubstate = SAVE_SCREEN_NO_CARD;
        }
      } else if (*cur == SAVE_STEP_WRITE) {
        int st;

        if (MemCardSync(1, &SM[SM_SYNC_CMD], &SM[SM_SYNC_RESULT]) == 0) {
          break;
        }
        st = SM[SM_SYNC_RESULT];
        if (st == McErrNone) {
          if (g_nSaveFileTag ==
              ((McFileHeader *)((char *)g_pRenderScratchPrimTop -
                                SAVE_IMAGE_BYTES))
                  ->clut[15]) {
            BuildSaveGameBuffer((char *)g_apRenderScratchPrimTopBlock[0] -
                                SAVE_IMAGE_BYTES);
            MemCardWrite(
                g_nSaveSlotSelected, g_szMemCardSaveName,
                (char *)g_apRenderScratchPrimTopBlock[0] - SAVE_IMAGE_BYTES,
                MEMCARD_SAVE_OFFSET(g_nSaveSlotIndex), SAVE_IMAGE_BYTES);
            *cur = SAVE_STEP_READ_BACK;
            break;
          }
          g_nSaveMenuSubstate = SAVE_SCREEN_NO_FILE;
        } else if (st == McErrFileNotExist) {
          g_nSaveMenuSubstate = SAVE_SCREEN_NO_FILE;
        } else {
          g_nSaveMenuSubstate = SAVE_SCREEN_ERROR;
        }
      } else if (*cur == SAVE_STEP_READ_BACK) {
        if (MemCardSync(1, &SM[SM_SYNC_CMD], &SM[SM_SYNC_RESULT]) == 0) {
          break;
        }
        if (SM[SM_SYNC_RESULT] == McErrNone) {
          MemCardRead(g_nSaveSlotSelected, g_szMemCardSaveName,
                      (char *)g_pRenderScratchPrimTop - SAVE_IMAGE_BYTES,
                      MEMCARD_SAVE_OFFSET(g_nSaveSlotIndex), SAVE_IMAGE_BYTES);
          *cur = SAVE_STEP_VERIFY;
          break;
        }
        g_nSaveMenuSubstate = SAVE_SCREEN_ERROR;
      } else {
        int v;

        if (MemCardSync(1, &SM[SM_SYNC_CMD], &SM[SM_SYNC_RESULT]) == 0) {
          break;
        }
        if (SM[SM_SYNC_RESULT] == McErrNone &&
            ComputeSaveGameChecksum((unsigned char *)g_pRenderScratchPrimTop -
                                    SAVE_IMAGE_BYTES) ==
                SAVE_RAW((char *)g_pRenderScratchPrimTop - SAVE_IMAGE_BYTES,
                         int, checksum)) {
          v = SAVE_SCREEN_SAVED;
        } else {
          v = SAVE_SCREEN_ERROR;
        }
        g_nSaveMenuSubstate = v;
      }
      g_nSaveMenuAnimPhase = 0;
    } break;
    case SAVE_SCREEN_NO_CARD:
    case SAVE_SCREEN_NO_FILE:
    case SAVE_SCREEN_ERROR:
      if (SM[SM_PHASE] >= 0x3C && (g_dwPadPressed & PAD_CROSS)) {
        PlaySoundEffect(LEVEL_SFX(SFX_MENU_SELECT), 0, SFX_MODE_PANNED, 0);
        SM[SM_PHASE] = 0;
        g_nSaveMenuSubstate = SAVE_SCREEN_FAILED;
        g_nSaveMenuCursor = SAVE_FAILED_RETRY;
      }
      break;
    case SAVE_SCREEN_FAILED:
      if (g_adwPadPressedBlock[0] & (PAD_UP | PAD_DOWN)) {
        PlaySoundEffect(LEVEL_SFX(SFX_MENU_MOVE), 0, SFX_MODE_PANNED, 0);
        SM[SM_CURSOR] = 1 - SM[SM_CURSOR];
      }
      if (SM[SM_PHASE] >= 0x20 && (g_adwPadPressedBlock[0] & PAD_CROSS)) {
        MemCardCloseSession();
        if (g_nSaveMenuCursor == SAVE_FAILED_RETRY) {
          PlaySoundEffect(LEVEL_SFX(SFX_MENU_SELECT), 0, SFX_MODE_PANNED, 0);
          SM[SM_PHASE] = 0;
          g_nSaveMenuSubstate = SAVE_SCREEN_SAVING;
          g_nSaveMenuCursor = SAVE_STEP_OPEN;
        } else {
          EndSaveMenuToWorld();
          PlaySoundEffect(LEVEL_SFX(SFX_MENU_SELECT), 0, SFX_MODE_PANNED, 0);
        }
      }
      break;
    case SAVE_SCREEN_SAVED:
      if (g_nSaveMenuAnimPhase >= 8 && (g_dwPadPressed & PAD_CROSS)) {
        MemCardCloseSession();
        EndSaveMenuToWorld();
        PlaySoundEffect(LEVEL_SFX(SFX_MENU_SELECT), 0, SFX_MODE_PANNED, 0);
      }
      break;
    }
    break;
  }
}
