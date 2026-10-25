#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* Update for gamestate 0xd (story-intro / new-game transition, 0x80032b08,
   0x6a4). Only runs its body while g_nTitleMode is 3; three substates:
   0 arms the transition (silences audio, picks the destination level from the
   message id — title screen, resume, or a fresh save's first level — and parks
   the camera on the intro shot), 1 blocks on the CD stream until the world is
   in RAM, 2 flies Spyro along the scripted intro path (two cosine sweeps with
   a hold and a linear drift between them), aiming him down the motion delta
   each frame, and hands off either to the gem cutscene (title path) or back to
   the level (resume path). Every substate falls through to the shared light
   rotation + timer tick. */

void Title_Update(void) {
  VECTOR delta;
  RECT clear;

  if (g_nTitleMode == 3) {
    if (g_nStoryIntroSubstate == 0) {
      StopAllSoundExceptMask(0);
      TickSpuPerFrame();
      g_nStoryIntroSubstate = 1;
      if (g_nStoryIntroMessage == 0) {
        g_nCurrentWorldId = 1;
        g_nCdStreamState = 0;
        g_pDrawBufA = g_pSharedLoadBuffer;
        g_pDrawBufB = g_pSharedLoadBuffer;
      } else if (g_nStoryIntroMessage == 1) {
        g_nFlightLevelActive = 0;
        g_nCdStreamState = 1;
        g_nReturnPortalLevelId = 0;
        g_nPortalTransitionActive = 0;
        g_nCurrentLevelId = g_nActiveLevelId;
      } else if (g_nStoryIntroMessage == 2) {
        InitNewGameState();
        g_nFlightLevelActive = 0;
        g_nCdStreamState = 1;
        g_nReturnPortalLevelId = 0;
        g_nPortalTransitionActive = 0;
        g_nActiveLevelId = g_anDeathTryLevelIds[g_nDeathTryIndex];
        g_nCurrentLevelId = g_nActiveLevelId;
      }
      g_nCameraEulerYaw = 0x400;
      g_nCameraEulerRoll = 0;
      g_nCameraEulerPitch = 0;
      g_anCameraPos[0] = 0x2000;
      g_anCameraPos[1] = 0;
      g_anCameraPos[2] = 0x2000;
      ResetSpyroEntity(1);
      g_anSpyroWorldPos[0] = 0x2780;
      g_anSpyroWorldPos[1] = 0xf00;
      g_anSpyroWorldPos[2] = 0x29a0;
      g_bSpyroAnimPrev = 0x10;
      g_bSpyroAnimCurrent = 0x10;
      g_abSpyroPersistentEuler[0] = 0;
      g_abSpyroPersistentEuler[1] = 0;
      g_abSpyroPersistentEuler[2] = 0;
      g_bSpyroAnimFrame = 1;
    } else if (g_nStoryIntroSubstate == 1) {
      if (g_nStoryIntroMessage == 0) {
        while (g_nCdStreamState < 3) {
          TickWorldBundleLoadStream();
          TickCdMusicStream();
        }
      } else {
        while (g_nCdStreamState < 6) {
          TickLevelTransitionStream(1);
          TickCdMusicStream();
        }
      }
      g_nStoryIntroSubstate = 2;
      g_nStoryIntroTimer = 0;
    } else if (g_nStoryIntroSubstate == 2) {
      int *pos = g_anSpyroWorldPosBlock;

      CopyVector(&delta, (VECTOR *)pos);
      if (g_nStoryIntroTimer < 0x50) {
        pos[0] = LookupCosine((-g_nStoryIntroTimer << 10) / 80) + 0x1780;
        g_nSpyroWorldPosY =
            (LookupSine((-g_nStoryIntroTimer << 10) / 80) >> 1) + 0xe80;
        g_nSpyroWorldPosZ =
            (LookupSine((-g_nStoryIntroTimer << 10) / 80) >> 1) + 0x2980;
      } else if (g_nStoryIntroTimer < 0x78) {
        pos[0] = 0x1400;
        g_nSpyroWorldPosY = 0xc00;
        g_nSpyroWorldPosZ = 0x20c0;
      } else if (g_nStoryIntroTimer < 0xc8) {
        pos[0] += 0x4c;
      } else if (g_nStoryIntroTimer < 0xf0) {
        pos[0] = 0x2880;
        g_nSpyroWorldPosY = 0x680;
        g_nSpyroWorldPosZ = 0x2180;
      } else if (g_nStoryIntroTimer < 0x140) {
        pos[0] =
            LookupCosine(0xc00 - (((g_nStoryIntroTimer - 0xf0) << 10) / 80)) +
            0x2880;
        g_nSpyroWorldPosY =
            (LookupSine(0xc00 - (((g_nStoryIntroTimer - 0xf0) << 10) / 80)) >>
             1) +
            0xe80;
        g_nSpyroWorldPosZ =
            (LookupSine((((g_nStoryIntroTimer - 0xf0) << 10) / 80) + 0xc00) >>
             1) +
            0x2980;
      } else {
        pos[0] = 0x2000;
        g_nSpyroWorldPosY = 0x1000;
        g_nSpyroWorldPosZ = 0x4000;
      }

      SubtractVector(&delta, (VECTOR *)g_anSpyroWorldPos, &delta);
      g_abSpyroPersistentEuler[1] =
          ArcTan2(VectorLength(&delta, 0), delta.vz, 0);
      g_abSpyroPersistentEuler[2] = ArcTan2(delta.vx, delta.vy, 0);
      AdvanceSpyroAnimFrame(3);
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();

      {
        int *msg = g_anStoryIntroMessageBlock;

        if (msg[0] == 0) {
          if (g_nCdStreamState < 7) {
            TickWorldBundleLoadStream();
          }
          if (g_nStoryIntroTimer >= 0x180 && g_nCdStreamState == 7) {
            clear.x = 0;
            clear.y = 0;
            clear.w = 0x200;
            clear.h = 0x1e0;
            ClearImage(&clear, 0, 0, 0);
            DrawSync(0);
            g_pWorkAreaTop = (void *)(0x80200000 - g_nCrt0StackSize);
            InitActorMeshScratchRegions(1);
            while (g_nCdStreamState < 0xa) {
              TickWorldBundleLoadStream();
              TickCdMusicStream();
            }
            BeginGemCutscene();
            g_nGamePaused = 1;
            return;
          }
        } else {
          if (g_nCdStreamState < 0xd) {
            TickLevelTransitionStream(1);
          }
          if (g_nStoryIntroTimer >= 0x180 && g_nCdStreamState == 0xd) {
            if (msg[0] == 2) {
              g_nDeathState = 1;
              g_nDeathFadeFrame = 0;
            }
            ResetSpyroEntity(1);
            TickLevelTransitionStream(1);
            return;
          }
        }
      }
    }
  }

  RotateLightVectorXZ(3);
  g_anStoryIntroTimerBlock[0] += 1;
}
