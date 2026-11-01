#include "cd_stream.h"
#include "cutscene_id.h"
#include "funcs.h"
#include "gamestate.h"
#include "globals.h"
#include "main_shared.h"
#include "title_state.h"

/* Update for gamestate 0xd (story-intro / new-game transition, 0x80032b08,
   0x6a4). Only runs its body while g_nTitleMode is TITLE_MODE_STORY_INTRO;
   three substates: 0 arms the transition (silences audio, picks the destination
   level from the message id — title screen, resume, or the attract demo's level
   — and parks the camera on the intro shot), 1 blocks on the CD stream until
   the world is in RAM, 2 flies Spyro along the scripted intro path (two cosine
   sweeps with a hold and a linear drift between them), aiming him down the
   motion delta each frame, and hands off either to the title cutscene (title
   path) or back to the level (resume path). Every substate falls through to the
   shared light rotation + timer tick. */

void StoryIntro_Update(void) {
  VECTOR delta;
  RECT clear;

  if (g_nTitleMode == TITLE_MODE_STORY_INTRO) {
    if (g_nStoryIntroSubstate == STORY_INTRO_ARM) {
      StopAllSoundExceptMask(0);
      TickSpuPerFrame();
      g_nStoryIntroSubstate = STORY_INTRO_STREAM;
      if (g_nTitleExitPath == TITLE_EXIT_NEW_GAME) {
        g_nCutsceneId = CUTSCENE_OPENING;
        g_nCdStreamState = CSTREAM_READ_HEADER;
        g_pDrawBufA = g_pSharedLoadBuffer;
        g_pDrawBufB = g_pSharedLoadBuffer;
      } else if (g_nTitleExitPath == TITLE_EXIT_LOADED_GAME) {
        g_nFlightLevelActive = 0;
        g_nCdStreamState = LSTREAM_FRESH;
        g_nReturnPortalLevelId = 0;
        g_nPortalTransitionActive = 0;
        g_nCurrentLevelId = g_nActiveLevelId;
      } else if (g_nTitleExitPath == TITLE_EXIT_DEMO) {
        InitNewGameState();
        g_nFlightLevelActive = 0;
        g_nCdStreamState = LSTREAM_FRESH;
        g_nReturnPortalLevelId = 0;
        g_nPortalTransitionActive = 0;
        g_nActiveLevelId = g_anDemoLevelIds[g_nDemoIndex];
        g_nCurrentLevelId = g_nActiveLevelId;
      }
      g_nCameraEulerYaw = 0x400;
      g_nCameraEulerRoll = 0;
      g_nCameraEulerPitch = 0;
      g_anCameraPos[0] = 0x2000;
      g_anCameraPos[1] = 0;
      g_anCameraPos[2] = 0x2000;
      ResetSpyroEntity(RESET_SPYRO_FULL);
      g_anSpyroWorldPos[0] = 0x2780;
      g_anSpyroWorldPos[1] = 0xf00;
      g_anSpyroWorldPos[2] = 0x29a0;
      g_bSpyroAnimPrev = 0x10;
      g_bSpyroAnimCurrent = 0x10;
      g_abSpyroPersistentEuler[0] = 0;
      g_abSpyroPersistentEuler[1] = 0;
      g_abSpyroPersistentEuler[2] = 0;
      g_bSpyroAnimFrame = 1;
    } else if (g_nStoryIntroSubstate == STORY_INTRO_STREAM) {
      if (g_nTitleExitPath == TITLE_EXIT_NEW_GAME) {
        while (g_nCdStreamState < CSTREAM_READ_AUDIO) {
          TickCutsceneBundleLoadStream();
          TickCdMusicStream();
        }
      } else {
        while (g_nCdStreamState < LSTREAM_UPLOAD_AUDIO) {
          TickLevelTransitionStream(1);
          TickCdMusicStream();
        }
      }
      g_nStoryIntroSubstate = STORY_INTRO_FLY;
      g_nStoryIntroTimer = 0;
    } else if (g_nStoryIntroSubstate == STORY_INTRO_FLY) {
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
          ArcTan2(VectorLength(&delta, VEC_LEN_XY), delta.vz, ATAN_ANGLE8);
      g_abSpyroPersistentEuler[2] = ArcTan2(delta.vx, delta.vy, ATAN_ANGLE8);
      AdvanceSpyroAnimFrame(3);
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();

      {
        int *path = g_anTitleExitPathBlock;

        if (path[0] == TITLE_EXIT_NEW_GAME) {
          if (g_nCdStreamState < CSTREAM_EXT_SECOND_HALF) {
            TickCutsceneBundleLoadStream();
          }
          if (g_nStoryIntroTimer >= 0x180 &&
              g_nCdStreamState == CSTREAM_EXT_SECOND_HALF) {
            clear.x = 0;
            clear.y = 0;
            clear.w = 0x200;
            clear.h = 0x1e0;
            ClearImage(&clear, 0, 0, 0);
            DrawSync(0);
            g_pWorkAreaTop = (void *)(0x80200000 - g_nCrt0StackSize);
            InitActorMeshScratchRegions(1);
            while (g_nCdStreamState < CSTREAM_DONE) {
              TickCutsceneBundleLoadStream();
              TickCdMusicStream();
            }
            BeginCutscene();
            g_nGamePaused = 1;
            return;
          }
        } else {
          if (g_nCdStreamState < LSTREAM_FINALIZE) {
            TickLevelTransitionStream(1);
          }
          if (g_nStoryIntroTimer >= 0x180 &&
              g_nCdStreamState == LSTREAM_FINALIZE) {
            if (path[0] == TITLE_EXIT_DEMO) {
              g_nDemoMode = DEMO_PLAYBACK;
              g_nDemoExitFadeFrame = 0;
            }
            ResetSpyroEntity(RESET_SPYRO_FULL);
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
