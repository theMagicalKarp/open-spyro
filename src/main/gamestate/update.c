#include "actor.h"
#include "actor_cutscene.h"
#include "actor_states.h"
#include "actor_type.h"
#include "angle.h"
#include "camera.h"
#include "cd_stream.h"
#include "credits_mode.h"
#include "cutscene_id.h"
#include "cutscene_path.h"
#include "dragon_rescue.h"
#include "emit_record.h"
#include "funcs.h"
#include "gamestate.h"
#include "globals.h"
#include "gpu_prim.h"
#include "hud.h"
#include "level_id.h"
#include "level_sfx.h"
#include "main_shared.h"
#include "match_asm.h"
#include "model.h"
#include "pad.h"
#include "pad_buttons.h"
#include "pause_menu.h"
#include "sample_bank.h"
#include "save_game.h"
#include "save_menu.h"
#include "sky.h"
#include "spu.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "title_state.h"
#include "types.h"
#include "vecmath.h"

/* GS_LEVEL_LOAD per-frame update (the level-intro variant of the gameplay
   tick): runs the glide-hold Spyro tick (TickSpyroFrameGlideHold), camera, and
   gem-tally particles, then advances the level-transition stream unless the CD
   stream is past stage 0xB and the tally is still active. (0x8002df9c, 100
   bytes.) */
void LevelLoad_Update(void) {
  TickSpyroFrameGlideHold();
  UpdateCameraFrame();
  AnimateGemTallyParticles();
  if (g_nCdStreamState < LSTREAM_READ_PATHS ||
      g_nLevelTransitionTallyActive == 0) {
    TickLevelTransitionStream(1);
  }
}

/* 0x8002e000 (132 bytes) — the camera-settle gamestate tick: run one gameplay
   frame and one camera frame, wrap the camera pitch back into signed 12-bit
   range, and drop out to GS_GAMEPLAY once the camera has either pitched below
   -0x200 or handed control back to the mode-12 parameter block. */
void CameraSettle_Update(void) {
  int pitch;

  TickSpyroGameplayFrame();
  UpdateCameraFrame();
  pitch = g_anCameraEulerPitchBlock[0] & ANGLE12_MASK;
  g_anCameraEulerPitchBlock[0] = pitch;
  if (pitch > 0x800) {
    g_anCameraEulerPitchBlock[0] = pitch - 0x1000;
  }
  if ((g_anCameraEulerPitchBlock[0] < -0x200) ||
      (g_pCameraTargetParams == g_anCameraParamTransitionGlideIn)) {
    g_nGamestate = GS_GAMEPLAY;
  }
}

/* Spyro state-delay updater (0x8002e084): tick Spyro with no input and the
   camera, counting idle frames. At 0x20 frames the counter wraps and bumps
   the transition counter (the second wrap returns to the world hub); at
   exactly 0x10 frames with no transition pending, kick Spyro to SPYRO_ST_GLIDE.
 */

void SpyroStateDelay_Update(void) {
  int frames;
  TickSpyroFrameNoInput();
  UpdateCameraFrame();
  frames = g_nMenuTickFrames + 1;
  g_nMenuTickFrames = frames;
  if (frames >= 0x20) {
    int passes;
    g_nMenuTickFrames = 0;
    passes = g_nMenuDrawFrames + 1;
    g_nMenuDrawFrames = passes;
    if (passes == 2) {
      BeginWorldHubReturn();
    }
  } else if (frames == 0x10) {
    if (g_nMenuDrawFrames == 0) {
      ChangeSpyroState(SPYRO_ST_GLIDE);
    }
  }
}

extern unsigned char g_abTitlescreenOverlayEnd_blk[]; /* first free byte past
                                                     the titlescreen overlay */
extern int
    g_nCdAudioVolume_blk[]; /* [0] music volume, -0x31c bytes SpuCommonAttr */

#define SFX(off) PlaySoundEffect(LEVEL_SFX(off), 0, SFX_MODE_PANNED, 0)

/* 0x8002e12c (0xa00): GS_PAUSE_MENU tick. Substate 0 is the main pause menu
   (resume / options / inventory / quit), 1 the options page (SFX and music
   volume, mono/stereo, vibration, camera mode) and 2 the quit confirmation,
   which on "yes" tears the level down and reloads the title world. */
void PauseMenu_Update(void) {
  RECT r;
  unsigned int pad;
  int c;
  int last;

  if (!g_nFlightLevelActive) {
    UpdateHudCounters();
  }
  g_nHudLevelGemHoldTimer = 0;
  g_nHudDragonHoldTimer = 0;
  g_nHudLivesHoldTimer = 0;
  g_nHudEggHoldTimer = 0;
  g_nHudKeyHoldTimer = 0;
  g_nMenuTickFrames++;
  RotateLightVectorXZ(3);
  if (g_nMenuDrawFrames < 5) {
    return;
  }

  pad = g_dwPadPressed;
  if (pad & PAD_SELECT) {
    EnterInventoryMenu(0);
    return;
  }

  if (g_nPauseMenuSubstate == PAUSE_PAGE_QUIT_CONFIRM) {
    if (pad & (PAD_RIGHT | PAD_LEFT)) {
      SFX(SFX_MENU_SELECT);
      g_nPauseMenuCursor = 1 - g_nPauseMenuCursor;
      return;
    }
    if (!(pad & (PAD_TRIANGLE | PAD_CROSS))) {
      return;
    }
    if (g_nPauseMenuCursor == PAUSE_QUIT_NO || (pad & PAD_TRIANGLE)) {
      SFX(SFX_MENU_MOVE);
      g_nPauseMenuSubstate = PAUSE_PAGE_MAIN;
      g_nPauseMenuCursor = PAUSE_ROW_EXIT;
      return;
    }
    StopAllSoundExceptMask(0);
    SFX(SFX_MENU_MOVE);
    TickSpuPerFrame();
    r.x = 0;
    r.y = 0;
    r.w = SCREEN_W;
    r.h = SCREEN_H * 2;
    ClearImage(&r, 0, 0, 0);
    DrawSync(0);
    g_pLoadStagingBuffer = g_abTitlescreenOverlayEnd_blk;
    g_pLevelDataBuffer = g_abTitlescreenOverlayEnd_blk;
    g_nCdStreamState = CSTREAM_READ_HEADER;
    g_nCutsceneId = CUTSCENE_TITLE_BACKDROP;
    {
      int lba = *(volatile int *)&g_nCdBaseLba;
      int count = *(volatile int *)&g_nLevelLoadChunkSize;
      int offset = *(volatile int *)&g_nLevelLoadByteOffset;
      void *dst = *(void *volatile *)&g_pSharedLoadBuffer;

      CdReadSyncSectors(lba, dst, count, offset, CD_STALL_TIMEOUT_TICKS);
    }
    while (g_nCdStreamState < CSTREAM_DONE) {
      TickCutsceneBundleLoadStream();
      TickCdMusicStream();
    }
    BeginCutscene();
    BeginStoryIntro();
    g_nSkipGamestateDraw = 1;
    return;
  }

  if (g_nPauseMenuSubstate == PAUSE_PAGE_OPTIONS) {
    if (pad & PAD_DOWN) {
      SFX(SFX_MENU_MOVE);
      c = g_nPauseMenuCursor;
      g_nMenuTickFrames = 0;
      g_nPauseMenuCursor = c + 1;
      if (c + 1 >= OPTIONS_ROW_COUNT) {
        g_nPauseMenuCursor = OPTIONS_ROW_SFX_VOLUME;
      } else if (c + 1 == OPTIONS_ROW_VIBRATION && !g_nPadIsDualshockFlag) {
        g_nPauseMenuCursor = c + 2;
      }
    } else if (pad & PAD_UP) {
      SFX(SFX_MENU_MOVE);
      c = g_nPauseMenuCursor;
      g_nMenuTickFrames = 0;
      g_nPauseMenuCursor = c - 1;
      if (c - 1 < 0) {
        g_nPauseMenuCursor = OPTIONS_ROW_DONE;
      } else if (c - 1 == OPTIONS_ROW_VIBRATION && !g_nPadIsDualshockFlag) {
        g_nPauseMenuCursor = c - 2;
      }
    }
    if (g_dwPadPressed & PAD_TRIANGLE) {
      goto back;
    }
    switch (g_nPauseMenuCursor) {
    case OPTIONS_ROW_SFX_VOLUME:
      if ((g_dwPadPressed & PAD_LEFT) && g_nSfxVolume > 0) {
        SFX(SFX_MENU_SELECT);
        g_nSfxVolume--;
      } else if ((g_dwPadPressed & PAD_RIGHT) && g_nSfxVolume < VOLUME_STEPS) {
        SFX(SFX_MENU_SELECT);
        g_nSfxVolume++;
      }
      g_nSfxMixVolume_blk[0] = g_nSfxVolume * SPU_VOLUME_MAX / VOLUME_STEPS;
      g_nSoundMasterVolume = (g_nSfxMixVolume_blk[0] << 12) / SPU_VOLUME_MAX;
      return;
    case OPTIONS_ROW_MUSIC_VOLUME:
      if ((g_dwPadPressed & PAD_LEFT) && g_nMusicVolume > 0) {
        SFX(SFX_MENU_SELECT);
        g_nMusicVolume--;
      } else if ((g_dwPadPressed & PAD_RIGHT) &&
                 g_nMusicVolume < VOLUME_STEPS) {
        SFX(SFX_MENU_SELECT);
        g_nMusicVolume++;
      }
      {
        int *p = g_nCdAudioVolume_blk;
        int *common = (int *)((char *)p - 0x31C);
        int vol;

        *common = SPU_COMMON_CDVOL;
        vol = g_nMusicVolume << MUSIC_VOLUME_SHIFT;
        *p = vol;
        g_nCdDaVolumeCurrent = vol;
        g_nSpuCommonCdVolumeR = vol;
        SetSpuCommonAttr((unsigned char *)common);
      }
      return;
    case OPTIONS_ROW_SPEAKERS:
      if (g_dwPadPressed & (PAD_CROSS | PAD_RIGHT | PAD_LEFT)) {
        SFX(SFX_MENU_SELECT);
        if ((*g_anSoundMonoMixFlagBlock = 1 - *g_anSoundMonoMixFlagBlock) !=
            0) {
          g_bCdMixRightToLeft = CD_MIX_HALF;
          g_bCdMixRightToRight = CD_MIX_HALF;
          g_bCdMixLeftToRight = CD_MIX_HALF;
          g_bCdMixLeftToLeft = CD_MIX_HALF;
        } else {
          g_bCdMixRightToRight = CD_MIX_FULL;
          g_bCdMixLeftToLeft = CD_MIX_FULL;
          g_bCdMixRightToLeft = 0;
          g_bCdMixLeftToRight = 0;
        }
        CdMix(&g_bCdMixLeftToLeft);
      }
      return;
    case OPTIONS_ROW_VIBRATION:
      if (g_nPadIsDualshockFlag) {
        if (g_dwPadPressed & (PAD_CROSS | PAD_RIGHT | PAD_LEFT)) {
          SFX(SFX_MENU_SELECT);
          g_nOptionVibrationEnabled = 1 - g_nOptionVibrationEnabled;
          if (g_nOptionVibrationEnabled &&
              g_nVibrationLevel < VIBRATION_TEST_LEVEL) {
            g_nVibrationLevel = VIBRATION_TEST_LEVEL;
          }
        }
      } else {
        g_nPauseMenuCursor = OPTIONS_ROW_CAMERA;
      }
      return;
    case OPTIONS_ROW_CAMERA:
      if (g_dwPadPressed & (PAD_CROSS | PAD_RIGHT | PAD_LEFT)) {
        SFX(SFX_MENU_SELECT);
        if (g_dwActiveCameraOptions == CAM_OPT_ACTIVE) {
          g_dwActiveCameraOptions = CAM_OPT_PASSIVE;
        } else {
          g_dwActiveCameraOptions = CAM_OPT_ACTIVE;
        }
      }
      return;
    case OPTIONS_ROW_DONE:
      if (g_dwPadPressed & PAD_CROSS) {
      back:
        SFX(SFX_MENU_SELECT);
        g_nPauseMenuSubstate = PAUSE_PAGE_MAIN;
        g_nPauseMenuCursor = PAUSE_ROW_CONTINUE;
      }
      return;
    }
    return;
  }

  last = PAUSE_ROW_EXIT;
  if (pad & PAD_DOWN) {
    SFX(SFX_MENU_MOVE);
    g_nMenuTickFrames = 0;
    if (last < ++g_nPauseMenuCursor) {
      g_nPauseMenuCursor = PAUSE_ROW_CONTINUE;
    }
  } else if (pad & PAD_UP) {
    SFX(SFX_MENU_MOVE);
    g_nMenuTickFrames = 0;
    if (--g_nPauseMenuCursor < 0) {
      g_nPauseMenuCursor = last;
    }
  }
  if (!(g_dwPadPressed & (PAD_CROSS | PAD_START))) {
    return;
  }
  switch (g_nPauseMenuCursor) {
  case PAUSE_ROW_CONTINUE:
    SFX(SFX_MENU_SELECT);
    ExitPauseMenuToGame(1);
    return;
  case PAUSE_ROW_OPTIONS:
    SFX(SFX_MENU_SELECT);
    g_nPauseMenuSubstate = PAUSE_PAGE_OPTIONS;
    g_nPauseMenuCursor = OPTIONS_ROW_SFX_VOLUME;
    return;
  case PAUSE_ROW_INVENTORY:
    EnterInventoryMenu(0);
    return;
  case PAUSE_ROW_EXIT:
    SFX(SFX_MENU_SELECT);
    if (LEVEL_IN_WORLD(g_nActiveLevelId) == LEVEL_HOME_SLOT) {
      g_nPauseMenuCursor = PAUSE_QUIT_NO;
      g_nPauseMenuSubstate = PAUSE_PAGE_QUIT_CONFIRM;
      return;
    }
    ZeroVector(&g_nSpyroSwingEulerTargetX);
    g_nSpyroScriptedMoveMode = 0;
    if (g_nFlightLevelActive) {
      g_nGamestate = GS_FLIGHT_RESULTS;
      g_nPauseMenuCursor = 0;
      g_nMenuTickFrames = 0;
      g_nFlightQuitFlag = 1;
      g_pfnLevelUpdateActors();
    } else {
      BeginPauseToMenuTransition();
    }
    return;
  }
}

#undef SFX

/* Inventory sub-menu per-frame update (0x8002eb2c, 0x2c4). Advances the
   HUD icon spin angle / frame-cycle counters and the pause-menu idle timer,
   spins the menu light vector, then (once the 5-frame transition is
   done) either runs level-select input or the normal sub-menu: cheat
   detection, up/down scrolling between visited levels (0x1C0 fixed-step
   scroll with cursor wrap), and pause/exit pad handling. */

void SubMenu_Update(void) {
  g_nHudFrameCycleMod9 = (g_nHudFrameCycleMod9 + 1) % 9;
  g_adwHudIconSpinAngleBlock[0] =
      (g_adwHudIconSpinAngleBlock[0] - g_nFrameStep) & ANGLE8_MASK;
  g_nMenuTickFrames += g_nFrameStep;
  RotateLightVectorXZ(3);
  if (g_nMenuDrawFrames < 5) {
    return;
  }
  if (g_nLevelSelectModeActive != 0) {
    HandleLevelSelectInput();
    return;
  }
  CheckCheatCodeMatch();
  if (g_nSubMenuScrollOffset == 0) {
    if (g_nSubMenuCursor > 0) {
      if (g_abLevelVisitedFlagVirtualBase[g_nSubMenuCursor * 6] != 0) {
        if (g_dwPadHeld & PAD_LEFT) {
          PlaySoundEffect(LEVEL_SFX(SFX_MENU_PAGE), 0, SFX_MODE_PANNED, 0);
          g_nSubMenuScrollDelta = 0x40;
        }
      }
    }
    if (g_nSubMenuCursor < 5) {
      if (g_abLevelVisitedFlag[(g_nSubMenuCursor * 6) + 6] != 0) {
        if (g_dwPadHeld & PAD_RIGHT) {
          PlaySoundEffect(LEVEL_SFX(SFX_MENU_PAGE), 0, SFX_MODE_PANNED, 0);
          g_nSubMenuScrollDelta = -0x40;
        }
      }
    }
  }
  if (g_nSubMenuScrollDelta != 0) {
    g_nSubMenuScrollOffset += g_nSubMenuScrollDelta;
    if (g_nSubMenuScrollOffset == 0) {
      g_nSubMenuScrollDelta = 0;
    } else if (g_nSubMenuScrollOffset == (-0x1C0)) {
      g_nSubMenuScrollDelta = -0x40;
      g_nSubMenuScrollOffset = 0x1C0;
      g_nSubMenuCursor += 1;
    } else if (g_nSubMenuScrollOffset == 0x1C0) {
      g_nSubMenuScrollDelta = 0x40;
      g_nSubMenuScrollOffset = -0x1C0;
      g_nSubMenuCursor -= 1;
    }
  }
  if (g_dwPadPressed & (PAD_TRIANGLE | PAD_START)) {
    EnterPauseMenu(0);
  } else if (g_dwPadPressed & (PAD_CROSS | PAD_SELECT)) {
    if (g_nLevelSelectModeActive == 0) {
      ExitSubMenuToTitle();
    }
  }
}

/* 0x8002edf0 (1492 bytes) — update for GS_RESPAWN and GS_GAME_OVER.

   Three arms on g_nRespawnOrGameOverMode. Mode 0 is the load frame: after 16
   frames it either hands straight back to gameplay (when the death happened
   outside a game-over, g_nGamestate == GS_RESPAWN) or streams the game-over
   scene in behind the render scratch region, rebases its chunk pointer table,
   parks the camera on the fixed game-over shot and resets Spyro's
   inventory/anim state. Mode 1 plays the presentation: frames 0xb4..0x174 sweep
   Spyro around the dragon on a cosine path, then he lands and the "press start"
   window opens. Mode 2 waits out the level-transition stream and returns to
   gameplay.

   The game-over scene's byte size is read TWICE off one held base for the
   CdReadSyncSectors call, and the second read must be a VOLATILE load routed
   through the loop counter `i`: cse merges the two reads in every non-volatile
   form, and a single-set destination would be boosted to max priority by
   sched.c's birthing_insn_p and sink below the second base's `la`. */

void RespawnOrGameOver_Update(void) {
  int unused[8];
  int mode;
  if (g_nRespawnOrGameOverFrame == 0) {
    StopAllSoundExceptMask(0);
  }
  g_nRespawnOrGameOverFrame += 1;
  RotateLightVectorXZ(3);
  mode = g_nRespawnOrGameOverMode;
  if (mode == 0) {
    if (g_nRespawnOrGameOverFrame >= 0x10) {
      if (g_nGamestate == GS_RESPAWN) {
        EndRespawnToGameplay();
        LoadAndStartLevelFromCd();
      } else {
        volatile int *size = g_anGameOverSceneBlock;
        int lba;
        unsigned char *scene;
        unsigned char *p;
        int count;
        unsigned char *fog;
        int n;
        int i;
        lba = g_nCdBaseLba;
        i = *size;
        CdReadSyncSectors(lba, g_apRenderScratchRegionBlock[0] - (*size), i,
                          g_nGameOverSceneSectorOffset, CD_STALL_TIMEOUT_TICKS);
        p = g_apRenderScratchRegionBlock[0] - (*size);
        fog = (unsigned char *)(&g_anSkyBlock[4]);
        fog[0] = p[0];
        fog[1] = p[1];
        fog[2] = p[2];
        scene = p;
        g_FrameDrawEnv0.r0 = fog[0];
        g_FrameDrawEnv0.g0 = fog[1];
        g_FrameDrawEnv1.r0 = fog[0];
        g_FrameDrawEnv1.g0 = fog[1];
        g_FrameDrawEnv0.b0 = fog[2];
        g_FrameDrawEnv1.b0 = fog[2];
        p += 4;
        count = *((int *)p);
        p += 4;
        g_anSkyBlock[1] = (int)p;
        g_anSkyBlock[0] = count;
        for (i = 0; i < g_anSkyBlock[0]; i++) {
          *((int *)p) = ((int)scene) + (*((int *)p));
          p += 4;
        }

        g_anCameraPos[0] = 0x2800;
        g_anCameraPos[1] = 0x80;
        g_nCameraEulerRoll = 0;
        g_anCameraPos[2] = 0x800;
        g_nCameraEulerPitch = g_anGameOverCameraEuler[0];
        g_nCameraEulerYaw = g_anGameOverCameraEuler[1];
        ResetSpyroEntity(RESET_SPYRO_FULL);
        g_bSpyroAnimPrev = SPYRO_ANIMID_GLIDE;
        g_bSpyroAnimCurrent = SPYRO_ANIMID_GLIDE;
        g_bSpyroAnimFrame = 1;
        g_nExtraLives = 4;
        g_nSparxFoodCount = 0;
        g_nSparxHealth = SPARX_GOLD;
        g_nPortalTransitionActive = 0;
        g_nReturnPortalLevelId = 0;
        g_nGameOverLevelIndex = g_nLevelIndex;
        FillWord(&g_nSpawnAnchorValid, 0, 0x68);
        if (g_nActiveLevelId != (((lba = g_nActiveLevelId) / 10) * 10)) {
          g_nNextLevelId = (g_nActiveLevelId / 10) * 10;
          g_nCdStreamState = LSTREAM_SELECT_LEVEL;
          g_nEnteredPortalIdx = -1;
        } else {
          g_nCdStreamState = LSTREAM_READ_PATHS;
        }
        g_nRespawnOrGameOverMode = 1;
        g_nRespawnOrGameOverFrame = 0;
      }
    }
  } else if (mode == 1) {
    int t = g_nRespawnOrGameOverFrame - 0xb4;
    if (((unsigned int)t) < 0xc0) {
      int angle = ((t * 16) + 0xe00) & 0xfff;
      int swing = LookupCosine(angle);
      int radius = 0x87c - ((t * 1086) / 192);
      g_anSpyroWorldPos[0] = ((swing * radius) >> 12) + 0x2a00;
      g_anSpyroWorldPos[1] = ((LookupSine(angle) * radius) >> 12) + 0xc00;
      g_anSpyroWorldPos[2] = 0xb44 - (t * 6);
      g_abSpyroPersistentEuler[2] = (angle + 0x400) >> 4;
      g_abSpyroPersistentEuler[1] = 0;
      g_abSpyroPersistentEuler[0] = (t / 12) - 0x20;
      if (g_nRespawnOrGameOverFrame < 0x164) {
        AdvanceSpyroAnimFrame(3);
      } else {
        g_bSpyroAnimCurrent = SPYRO_ANIMID_TEETER;
        g_bSpyroAnimFrame = 0;
        g_bSpyroSubFrameTimer = g_nRespawnOrGameOverFrame - 0x64;
        g_abSpyroPersistentEuler[1] =
            -(((g_nRespawnOrGameOverFrame - 0x164) * 10) / 16);
        g_abSpyroPersistentEuler[0] = g_nRespawnOrGameOverFrame - 0x74;
      }
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
    }
    if (g_nRespawnOrGameOverFrame >= 0x174) {
      if (g_nRespawnOrGameOverFrame == 0x174) {
        g_bSpyroAnimPrev = SPYRO_ANIMID_TEETER;
        g_bSpyroAnimCurrent = SPYRO_ANIMID_TEETER;
        g_bSpyroFramePrev = 0;
        g_bSpyroAnimFrame = 1;
        g_bSpyroSubFrameTimer = 0;
      }
      g_anSpyroWorldPos[0] = 0x26f8;
      g_anSpyroWorldPos[1] = 0x900;
      g_anSpyroWorldPos[2] = 0x6c4;
      g_abSpyroPersistentEuler[1] = 0xf6;
      g_abSpyroPersistentEuler[0] = 0;
      g_abSpyroPersistentEuler[2] = 0xe0;
      AdvanceSpyroAnimFrame(3);
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
    }
    if (g_nCdStreamState < LSTREAM_READ_PATHS) {
      TickLevelTransitionStream(1);
    } else if ((g_nRespawnOrGameOverFrame >= 0x169) &&
               ((g_dwPadHeld & PAD_START) != 0)) {
      RestartSpyroAnimWithState(SPYRO_ST_IDLE);
      g_nRespawnOrGameOverMode = 2;
      g_nRespawnOrGameOverFrame = 0;
    }
  } else if (g_nRespawnOrGameOverFrame >= 0x11) {
    TickLevelTransitionStream(1);
    if (g_nCdStreamState < LSTREAM_PRESERVE) {
      EndRespawnToGameplay();
    }
  }
}

/* GS 6 per-frame update: just forwards to the substate no-op stub. */
void Gamestate06_Update(void) { Gamestate06SubstateStub_Noop(); }

/* Matching notes. Several accesses are spelled to control the scheduler's
   memory disambiguation (sched.c true/anti dependence: a non-QImode store
   through a varying in-struct address never conflicts with a fixed-address
   scalar access, and nothing else is disambiguated):
   - the case-4 sample-bank entry is written through a struct view (SBE), so
     the voice-attr globals that follow do not chain behind it; those
     globals and the source-actor read before them are in turn written as
     one-field struct views, so they keep the original's order (and the
     StopSoundVoiceByIndex `li a1,4` keeps the call's delay slot);
   - the case-0 timer base is the function-scope `i` (it lives in s1, as the
     original's base does), read into a block-local before the rumble store;
   - both voice-marker arguments are `&GB[GB_VOICE]`, which gives the case-6
   timer base its a3 preference and lets the case-0 and case-6 PlaySoundEffect
     tails cross-jump;
   - the case-2 5-holder is asm-opaque so it lands in the AllocActorRecordSlot
     delay slot, and case 4's `done` doubles as the case-5 table pointer (two
     sets, so sched1 does not boost it above AdvanceSpyroAnimFrame). */

/* The dragon-rescue state record at 0x80077058: [0] substate, [1] stream state,
   [5] timer, [6] sweep length, ... [24] statue, [25] mirror, [26] source. */

#define GB g_anDragonRescueBlock
/* Its words as the held-base accesses below read them (the array form keeps
 * the original's single base register). */
#define GB_TIMER 5     /* g_nDragonRescueTimer */
#define GB_VOICE 11    /* g_bSceneSfxVoiceMarker (the voice line's row) */
#define GB_PAD_HOLD 13 /* g_nDragonRescuePadHoldTimer */
#define GB_CAM_START_YAW                                                       \
  18 /* g_nDragonRescueCameraStartYaw (radius, z follow) */
#define GB_CAM_TARGET_YAW                                                      \
  21                 /* g_nDragonRescueCameraTargetYaw (radius, z follow) */
#define GB_STATUE 24 /* g_pDragonRescueStatueActor */
#define EUL2 (((unsigned char *)g_anSpyroWorldPosBlock)[0xE])
#define SRC (*(Actor *volatile *)&g_pDragonRescueSourceActor)
#define STATUE (*(Actor *volatile *)&g_pDragonRescueStatueActor)
#define MIRROR (*(Actor *volatile *)&g_pDragonRescueSpyroMirrorActor)
#define SBE ((SAMPLEENTRY *)g_apSampleBankEntriesBlock[0])
/* One 0x18-byte key of the dragon-rescue camera path
 * (g_pDragonRescueCameraPath). */
typedef struct CamPathKey {
  int x, y, z;
  int roll, pitch, yaw;
} CamPathKey;
/* A key field as a plain byte-offset int reference; the struct member form
 * reschedules the six camera stores. */
#define CAM_KEY_RAW(p, field)                                                  \
  (*(int *)((char *)(p) + (int)&((CamPathKey *)0)->field))

/* The first AnimationHeader of a Model. */
#define FIRST_ANIM(m) MODEL_RAW(m, int, m_Animations[0])
#define W32(p, off) (*(int *)((char *)(p) + (off)))
#define H16(p, off) (*(short *)((char *)(p) + (off)))
#define U8(p, off) (*(unsigned char *)((char *)(p) + (off)))
#define REC(p) (*(int **)(p))
/* The source statue's DragonStatueState (REC(p) as the typed record). */
#define STATUE_ST(p) ((DragonStatueState *)REC(p))
#define SRCS (((struct { char *volatile p; } *)&g_pDragonRescueSourceActor)->p)
#define OTY ((int)g_dwDragonRescueOrbitTargetYaw)

/* Timer ticks between keyframes of a mesh's animation for its frame rate:
 * the timer advances by g_nFrameStep (two per 30 Hz frame) and a rate of
 * ANIM_RATE_TICKS(n) steps a frame every n frames, so a keyframe lasts 2n. */
#define FRAME_STEP(model, step)                                                \
  {                                                                            \
    int rate = ((AnimationHeader *)(FIRST_ANIM(model)))->m_ProgressPerTick;    \
                                                                               \
    if (rate == ANIM_RATE_TICKS(4)) {                                          \
      step = 8;                                                                \
    } else if (rate == ANIM_RATE_TICKS(3)) {                                   \
      step = 6;                                                                \
    } else {                                                                   \
      step = 2;                                                                \
      if (rate == ANIM_RATE_TICKS(2)) {                                        \
        step = 4;                                                              \
      }                                                                        \
    }                                                                          \
  }

/* 0x8002f3e4 (0x20d0): the dragon-rescue gamestate (GS_DRAGON_RESCUE).
   Substates: 0 the aura star swells while Spyro keeps playing, 1 Spyro is swept
   round the statue and the camera eases in, 2 waits for the dragon assets and
   spawns the statue and Spyro's stand-in, 3 the statue turns to face its
   heading, 4 the camera path plays (with the dragon's voice line), 5 the
   statue lights up and the HUD icons roll in, 6 the dragon counter ticks,
   7 the fade back to the world. */
void DragonRescue_Update(void) {
  int vec[3];
  short ang[4];
  int t;
  int a;
  int d;
  int i;
  int done;

  GB[GB_TIMER] += g_nFrameStep;
  g_nDragonRescueSecondaryTimer += g_nFrameStep;
  switch (g_nDragonRescueSubstate) {
  case DRAGON_RESCUE_AURA: {
    int now;

    g_dwGamestateFrames++;
    g_pfnLevelUpdateActors();
    TickSpyroGameplayFrame();
    UpdateCameraFrame();
    g_nDragonRescueAuraStarEnabled = 1;
    g_bDragonRescueAuraStarColor =
        g_nDragonRescueTimer * RESCUE_AURA_COLOR_STEP + RESCUE_AURA_COLOR_BASE;
    g_nDragonRescueAuraStarRadius =
        g_nDragonRescueTimer * RESCUE_AURA_RADIUS_STEP;
    g_abDragonRescueAuraStarEuler[2] +=
        ((g_nDragonRescueTimer >> 4) + 2) * g_nFrameStep;
    if (g_nDragonRescueTimer > RESCUE_AURA_TINT_START) {
      g_nDragonRescueFullscreenTintLevel =
          (g_nDragonRescueTimer - RESCUE_AURA_TINT_START) *
          RESCUE_AURA_TINT_STEP;
    } else {
      g_nDragonRescueFullscreenTintLevel = 0;
    }
    if (g_nPulseRumbleTimer < RESCUE_RUMBLE_FRAMES) {
      g_nPulseRumbleTimer = RESCUE_RUMBLE_FRAMES;
    }
    i = (int)&GB[GB_TIMER];
    now = *(int *)i;
    g_nPulseRumbleAmount = RESCUE_RUMBLE_AMOUNT;
    if (now >= RESCUE_AURA_FRAMES) {
      int *rec;

      g_nDragonRescueSubstate = DRAGON_RESCUE_SWEEP;
      *(int *)i = 0;
      g_nDragonRescueFullscreenTintLevel = 0xFF;
      ACTOR_RAW(SRC, int, posZ) -= RESCUE_SOURCE_SINK;
      {
        Actor *src = SRC;

        g_nDragonRescueOrbitStartYaw =
            ArcTan2(g_anSpyroWorldPos[0] - src->posX,
                    g_nSpyroWorldPosY - src->posY, ATAN_ANGLE12);
      }
      SubtractVector(vec, g_anSpyroWorldPos, &SRC->posX);
      g_nDragonRescueOrbitStartRadius = VectorLength(vec, VEC_LEN_XY);
      {
        Actor *src = SRC;

        g_dwDragonRescueOrbitTargetYaw = STATUE_ST(src)->orbitYaw;
        g_nDragonRescueOrbitTargetRadius = STATUE_ST(src)->orbitRadius;
        g_nDragonRescueCameraStartYaw =
            ArcTan2(*(int *)(int)g_anCameraPos - ACTOR_RAW(src, int, posX),
                    g_anCameraPos[1] - ACTOR_RAW(src, int, posY), ATAN_ANGLE12);
      }
      SubtractVector(vec, g_anCameraPos, &SRC->posX);
      g_nDragonRescueCameraStartRadius = VectorLength(vec, VEC_LEN_XY);
      g_nDragonRescueCameraStartZ = g_anCameraPos[2];
      {
        Actor *src = SRC;

        rec = REC(src); /* DragonStatueState.camPos */
        g_nDragonRescueCameraTargetYaw = ArcTan2(
            W32(rec, 0) - src->posX, W32(rec, 4) - src->posY, ATAN_ANGLE12);
      }
      SubtractVector(vec, rec, &SRC->posX);
      g_nDragonRescueCameraTargetRadius = VectorLength(vec, VEC_LEN_XY);
      g_nDragonRescueCameraTargetZ = W32(rec, 8);
      g_nDragonRescueSweepDurationFrames =
          (AbsAngleDelta12(g_nDragonRescueOrbitStartYaw,
                           g_dwDragonRescueOrbitTargetYaw) >>
           4) +
          RESCUE_SWEEP_BASE_FRAMES;
      if (g_nDragonRescueSweepDurationFrames < RESCUE_SWEEP_MIN_FRAMES) {
        g_nDragonRescueSweepDurationFrames = RESCUE_SWEEP_MIN_FRAMES;
      }
      if (g_nDragonRescueSweepDurationFrames > RESCUE_SWEEP_MAX_FRAMES) {
        g_nDragonRescueSweepDurationFrames = RESCUE_SWEEP_MAX_FRAMES;
      }
      g_nSpyroSuperflameTimerSave = g_nSpyroSuperflameTimer;
      ResetSpyroEntity(RESET_SPYRO_FULL);
      g_bSpyroAnimPrev = SPYRO_ANIMID_WALK;
      g_bSpyroAnimCurrent = SPYRO_ANIMID_WALK;
      g_bSpyroAnimFrame = 1;
      g_bSpyroFramePrev = 0;
      g_bSpyroSubFrameTimer = 0;
      g_pEmitListWriteCursor = g_pEmitListBase;
      ((unsigned char *)g_pEmitListBase)[1] = EMIT_CLASS_END;
      StopAllSoundExceptMask(1 << SRC->voiceRow);
      PlaySoundEffect(LEVEL_SFX(SFX_RESCUE_SWEEP), 0, SFX_MODE_PANNED,
                      (unsigned char *)&GB[GB_VOICE]);
    }
  } break;
  case DRAGON_RESCUE_SWEEP:
    if (g_nDragonRescueTimer < RESCUE_AURA_FADE_FRAMES) {
      int k = RESCUE_AURA_FADE_FRAMES - g_nDragonRescueTimer;

      g_nDragonRescueAuraStarEnabled = 1;
      g_bDragonRescueAuraStarColor =
          k * RESCUE_AURA_COLOR_STEP + RESCUE_AURA_COLOR_BASE;
      g_nDragonRescueAuraStarRadius = k * RESCUE_AURA_RADIUS_STEP;
      g_abDragonRescueAuraStarEuler[2] += ((k >> 4) + 2) * g_nFrameStep;
    } else {
      g_nDragonRescueAuraStarEnabled = 0;
    }
    if (g_nDragonRescueTimer < 8) {
      g_nDragonRescueFullscreenTintLevel = (8 - g_nDragonRescueTimer) << 5;
    } else {
      g_nDragonRescueFullscreenTintLevel = 0;
    }
    {
      Actor *s = SRC;

      s->subMode = (s->subMode + 1) & 3;
    }
    {
      Actor *s = SRC;

      s->rotX = g_abDragonRescueFlickerPairs1[s->subMode * 2];
    }
    {
      Actor *s = SRC;

      s->rotY = g_abDragonRescueFlickerPairs1Y[s->subMode * 2];
    }
    if (g_nPulseRumbleTimer < RESCUE_RUMBLE_FRAMES) {
      g_nPulseRumbleTimer = RESCUE_RUMBLE_FRAMES;
    }
    g_nPulseRumbleAmount = RESCUE_RUMBLE_AMOUNT;
    g_pfnLevelSpawnActor(ACTOR_TYPE_STATUE_SHARD, g_pDragonRescueSourceActor);
    if (g_nDragonRescueTimer < g_nDragonRescueSweepDurationFrames) {
      t = LookupSine((g_nDragonRescueTimer << 11) /
                         g_nDragonRescueSweepDurationFrames -
                     0x400) +
          0x1000;
      if (ANGLE12_SHORT_WAY_UP(OTY, g_nDragonRescueOrbitStartYaw)) {
        a = g_nDragonRescueOrbitStartYaw +
            ((AbsAngleDelta12(OTY, g_nDragonRescueOrbitStartYaw) * t) >> 13);
      } else {
        a = g_nDragonRescueOrbitStartYaw -
            ((AbsAngleDelta12(OTY, g_nDragonRescueOrbitStartYaw) * t) >> 13);
      }
      d = g_nDragonRescueOrbitStartRadius +
          (((g_nDragonRescueOrbitTargetRadius -
             g_nDragonRescueOrbitStartRadius) *
            (LookupSine((g_nDragonRescueTimer << 10) /
                            g_nDragonRescueSweepDurationFrames -
                        0x400) +
             0x1000)) >>
           12);
      CopyVector(vec, g_anSpyroWorldPos);
      g_anSpyroWorldPos[0] = SRC->posX + ((LookupCosine(a) * d) >> 12);
      g_nSpyroWorldPosY = SRC->posY + ((LookupSine(a) * d) >> 12);
      SubtractVector(vec, g_anSpyroWorldPos, vec);
      {
        int dir = ArcTan2(vec[0], vec[1], ATAN_ANGLE8);

        if (AbsAngleDelta8(dir, g_abSpyroPersistentEuler[2]) >= 4) {
          if (ANGLE8_SHORT_WAY_UP(dir, g_abSpyroPersistentEuler[2])) {
            g_abSpyroPersistentEuler[2] =
                g_abSpyroPersistentEuler[2] +
                (AbsAngleDelta8(dir, g_abSpyroPersistentEuler[2]) >> 2);
          } else {
            g_abSpyroPersistentEuler[2] =
                g_abSpyroPersistentEuler[2] -
                (AbsAngleDelta8(dir, g_abSpyroPersistentEuler[2]) >> 2);
          }
        }
      }
      g_nSpyroAnimPlayRate_blk[0] = VectorLength(vec, VEC_LEN_XY) >> 1;
      if (g_nSpyroAnimPlayRate_blk[0] < 4) {
        g_nSpyroAnimPlayRate_blk[0] = 4;
      }
      if (g_nSpyroAnimPlayRate_blk[0] > 0x10) {
        g_nSpyroAnimPlayRate_blk[0] = 0x10;
      }
      if (ANGLE12_SHORT_WAY_UP(GB[GB_CAM_TARGET_YAW], GB[GB_CAM_START_YAW])) {
        a = GB[GB_CAM_START_YAW] +
            ((AbsAngleDelta12(GB[GB_CAM_TARGET_YAW], GB[GB_CAM_START_YAW]) *
              t) >>
             13);
      } else {
        a = GB[GB_CAM_START_YAW] -
            ((AbsAngleDelta12(GB[GB_CAM_TARGET_YAW], GB[GB_CAM_START_YAW]) *
              t) >>
             13);
      }
      d = g_nDragonRescueCameraStartRadius +
          (((g_nDragonRescueCameraTargetRadius -
             g_nDragonRescueCameraStartRadius) *
            t) >>
           13);
      g_anCameraPos[0] = SRC->posX + ((LookupCosine(a) * d) >> 12);
      g_anCameraPos[1] = SRC->posY + ((LookupSine(a) * d) >> 12);
      g_anCameraPos[2] =
          g_nDragonRescueCameraStartZ +
          (((g_nDragonRescueCameraTargetZ - g_nDragonRescueCameraStartZ) * t) >>
           13);
      {
        int land = g_nDragonRescueSweepDurationFrames - RESCUE_LAND_LEAD_FRAMES;

        if (g_nDragonRescueTimer >= land &&
            g_nDragonRescueTimer - g_nFrameStep < land) {
          g_bSpyroFramePrev = g_bSpyroAnimFrame;
          g_bSpyroAnimCurrent = SPYRO_ANIMID_STOP;
          g_bSpyroAnimFrame = 0;
          g_bSpyroSubFrameTimer = 0;
          g_nSpyroAnimPlayRate = RESCUE_LAND_ANIM_RATE;
        }
      }
    } else if (g_nDragonRescueTimer <
               g_nDragonRescueSweepDurationFrames + RESCUE_SETTLE_FRAMES) {
      if (g_nDragonRescueTimer - g_nFrameStep <
          g_nDragonRescueSweepDurationFrames) {
        g_anSpyroWorldPos[0] =
            SRC->posX + ((LookupCosine(g_dwDragonRescueOrbitTargetYaw) *
                          g_nDragonRescueOrbitTargetRadius) >>
                         12);
        {
          int s = LookupSine(g_dwDragonRescueOrbitTargetYaw) *
                  g_nDragonRescueOrbitTargetRadius;
          Actor *src = SRC;
          int y = ACTOR_RAW(src, int, posY);

          g_nDragonRescueOrbitStartYaw = g_abSpyroPersistentEuler[2] << 4;
          g_dwDragonRescueOrbitTargetYaw =
              (g_dwDragonRescueOrbitTargetYaw + ANGLE12_HALF) & ANGLE12_MASK;
          g_nSpyroWorldPosY = y + (s >> 12);
          CopyVector(g_anCameraPos, REC(src));
        }
      }
      if (ANGLE12_SHORT_WAY_UP(OTY, g_nDragonRescueOrbitStartYaw)) {
        a = g_nDragonRescueOrbitStartYaw +
            (AbsAngleDelta12(OTY, g_nDragonRescueOrbitStartYaw) *
             (g_nDragonRescueTimer - g_nDragonRescueSweepDurationFrames)) /
                32;
      } else {
        a = g_nDragonRescueOrbitStartYaw -
            (AbsAngleDelta12(OTY, g_nDragonRescueOrbitStartYaw) *
             (g_nDragonRescueTimer - g_nDragonRescueSweepDurationFrames)) /
                32;
      }
      g_abSpyroPersistentEuler[2] = a >> 4;
      g_nSpyroAnimPlayRate = 8;
    } else {
      g_nDragonRescueSubstate = DRAGON_RESCUE_LOAD_WAIT;
      g_bSpyroSubFrameTimer = 4;
      g_nDragonRescueTimer = 0;
      g_bSpyroAnimCurrent = SPYRO_ANIMID_IDLE;
      g_bSpyroAnimFrame = 0;
      g_nSpyroAnimPlayRate = 4;
      g_abSpyroPersistentEuler[2] = (int)g_dwDragonRescueOrbitTargetYaw >> 4;
    }
    if (g_nDragonRescueSubstate == DRAGON_RESCUE_SWEEP) {

      AddVector(vec, g_anSpyroWorldPos, &SRC->posX);
      RShiftVector3(vec, 1);
      SubtractVector(vec, vec, g_anCameraPos);
      if (g_nDragonRescueTimer < 0x10) {
        ang[0] = 0;
        ang[1] = ArcTan2(VectorLength(vec, VEC_LEN_XY), -vec[2], ATAN_ANGLE12);
        ang[2] = ArcTan2(vec[0], vec[1], ATAN_ANGLE12);
        g_nCameraEulerRoll = (g_nCameraEulerRoll +
                              ((SignedAngleDelta12(ang[0], g_nCameraEulerRoll) *
                                g_nDragonRescueTimer) >>
                               4)) &
                             ANGLE12_MASK;
        g_nCameraEulerPitch =
            (g_nCameraEulerPitch +
             ((SignedAngleDelta12(ang[1], g_nCameraEulerPitch) *
               g_nDragonRescueTimer) >>
              4)) &
            ANGLE12_MASK;
        g_nCameraEulerYaw = (g_nCameraEulerYaw +
                             ((SignedAngleDelta12(ang[2], g_nCameraEulerYaw) *
                               g_nDragonRescueTimer) >>
                              4)) &
                            ANGLE12_MASK;
      } else if (g_nDragonRescueTimer <
                 g_nDragonRescueSweepDurationFrames - 0x20) {
        g_nCameraEulerRoll = 0;
        g_nCameraEulerPitch =
            ArcTan2(VectorLength(vec, VEC_LEN_XY), -vec[2], ATAN_ANGLE12);
        g_nCameraEulerYaw = ArcTan2(vec[0], vec[1], ATAN_ANGLE12);
      } else {
        /* DragonStatueState.camEuler (+0x0C..+0x14) as byte-offset ints: the
           struct member form reschedules the three blends (A400) */
        int *rec = REC(SRC);

        ang[0] = 0;
        ang[1] = ArcTan2(VectorLength(vec, VEC_LEN_XY), -vec[2], ATAN_ANGLE12);
        ang[2] = ArcTan2(vec[0], vec[1], ATAN_ANGLE12);
        g_nCameraEulerRoll =
            (ang[0] +
             ((SignedAngleDelta12(W32(rec, 0xc), ang[0]) *
               (g_nDragonRescueTimer - g_nDragonRescueSweepDurationFrames +
                RESCUE_SETTLE_FRAMES)) >>
              6)) &
            ANGLE12_MASK;
        g_nCameraEulerPitch =
            (ang[1] +
             ((SignedAngleDelta12(W32(rec, 0x10), ang[1]) *
               (g_nDragonRescueTimer - g_nDragonRescueSweepDurationFrames +
                RESCUE_SETTLE_FRAMES)) >>
              6)) &
            ANGLE12_MASK;
        g_nCameraEulerYaw =
            (ang[2] +
             ((SignedAngleDelta12(W32(rec, 0x14), ang[2]) *
               (g_nDragonRescueTimer - g_nDragonRescueSweepDurationFrames +
                RESCUE_SETTLE_FRAMES)) >>
              6)) &
            ANGLE12_MASK;
      }
    }
    {
      vec[0] = (g_nSpyroGroundNormalX * LookupCosine(EUL2 << 4) +
                g_nSpyroGroundNormalY * LookupSine(EUL2 << 4)) >>
               12;
      vec[1] = (g_nSpyroGroundNormalY * LookupCosine(EUL2 << 4) -
                g_nSpyroGroundNormalX * LookupSine(EUL2 << 4)) >>
               12;
      vec[2] = g_anSpyroGroundNormal[2];
      g_abSpyroPersistentEuler[0] = -ArcTan2(
          IntegerSqrt(vec[0] * vec[0] + vec[2] * vec[2]), vec[1], ATAN_ANGLE12);
      g_abSpyroPersistentEuler[1] = -ArcTan2(vec[2], vec[0], ATAN_ANGLE12);
      AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
      g_nSpyroWorldPosZ += RESCUE_FLOOR_PROBE_RAISE;
      g_nSpyroGroundHeightZ = FindWorldFloorBelowPoint(g_anSpyroWorldPosBlock);
      g_nSpyroWorldPosZ = g_nSpyroGroundHeightZ + SPYRO_RADIUS;
      SampleSpyroShadowRingHeights(0);
    }
    break;
  case DRAGON_RESCUE_LOAD_WAIT: {
    Actor *s = SRC;

    s->subMode = (s->subMode + 1) & 3;
  }
    {
      Actor *s = SRC;

      s->rotX = g_abDragonRescueFlickerPairs2[s->subMode * 2];
    }
    {
      Actor *s = SRC;

      s->rotY = g_abDragonRescueFlickerPairs2Y[s->subMode * 2];
    }
    if (g_nPulseRumbleTimer < RESCUE_RUMBLE_FRAMES) {
      g_nPulseRumbleTimer = RESCUE_RUMBLE_FRAMES;
    }
    g_nPulseRumbleAmount = RESCUE_RUMBLE_AMOUNT;
    g_pfnLevelSpawnActor(ACTOR_TYPE_STATUE_SHARD, g_pDragonRescueSourceActor);
    if (g_nDragonRescueTimer < RESCUE_LOAD_MIN_FRAMES ||
        g_nDragonRescueStreamState < RESCUE_STREAM_RELOAD_LEVEL) {
      AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
    } else {
      g_nDragonRescueSubstate = DRAGON_RESCUE_TURN;
      g_nDragonRescueTimer = 0;
      {
        int n;
        int five;

        for (n = 0; n < RESCUE_SHARD_BURST; n++) {
          g_pfnLevelSpawnActor(ACTOR_TYPE_STATUE_SHARD,
                               g_pDragonRescueSourceActor);
        }
        OPAQUE_COPY(five, 5);
        GB[GB_STATUE] = (int)AllocActorRecordSlot();
        InitActorRenderDefaults((void *)GB[GB_STATUE]);
        ACTOR_RAW(STATUE, short, type) = ACTOR_TYPE_RESCUED_DRAGON;
        CopyVector(&((Actor *)GB[GB_STATUE])->posX, &SRC->posX);
        {
          Actor *s = SRC;
          Actor *t = STATUE;

          t->rotZ = s->rotZ;
        }
        STATUE->scale = RESCUE_DRAGON_START_SCALE;
        STATUE->otBias = five;
        ACTOR_RAW(STATUE, int, groundCode) = -1;
        STATUE->posZ += RESCUE_FLOOR_PROBE_RAISE;
        FindWorldFloorBelowPoint(&((Actor *)GB[GB_STATUE])->posX);
        STATUE->posZ -= RESCUE_FLOOR_PROBE_RAISE;
        EncodeHitNormalToActorGroundCode((void *)GB[GB_STATUE]);
        InsertActorIntoSpatialGrid((void *)GB[GB_STATUE]);
        g_pDragonRescueSpyroMirrorActor = AllocActorRecordSlot();
        InitActorRenderDefaults(g_pDragonRescueSpyroMirrorActor);
        ACTOR_RAW(MIRROR, short, type) = ACTOR_TYPE_RESCUE_SPYRO;
        CopyVector(&MIRROR->posX, g_anSpyroWorldPos);
        MIRROR->otBias = five;
        InsertActorIntoSpatialGrid(MIRROR);
        StopSoundVoiceByIndex(GB[GB_VOICE], VOICE_RELEASE);
        PlaySoundEffect(LEVEL_SFX(SFX_RESCUE_STATUE), 0, SFX_MODE_PANNED,
                        (unsigned char *)&GB[GB_VOICE]);
        ACTOR_RAW(SRC, int, posZ) += RESCUE_SOURCE_SINK;
        DespawnActorRecord(SRC);
      }
    }
    break;
  case DRAGON_RESCUE_TURN:
    if (GB[GB_TIMER] < RESCUE_TURN_FRAMES) {
      STATUE->scale =
          RESCUE_DRAGON_START_SCALE - GB[GB_TIMER] * RESCUE_DRAGON_GROW_STEP;
      {
        Actor *s = SRC;
        int turn;

        turn = SignedAngleDelta8(STATUE_ST(s)->dragonYaw >> 4, s->rotZ) *
               GB[GB_TIMER] / RESCUE_TURN_FRAMES;
        {
          Actor *s2 = SRC;
          Actor *t = STATUE;

          t->rotZ = s2->rotZ + turn;
        }
      }
    } else {
      Actor *s = SRC;
      Actor *t;

      g_nDragonRescueSubstate = DRAGON_RESCUE_CAMERA_PATH;
      GB[GB_TIMER] = 0;
      t = STATUE;
      t->rotZ = STATUE_ST(s)->dragonYaw >> 4;
      STATUE->scale = 0;
      MIRROR->rotX = g_abSpyroPersistentEuler[0];
      MIRROR->rotY = g_abSpyroPersistentEuler[1];
      MIRROR->rotZ = g_abSpyroPersistentEuler[2];
    }
    break;
  case DRAGON_RESCUE_CAMERA_PATH: {
    int save[5];
    int step;

    done = 0;
    if (GB[GB_TIMER] < g_nDragonRescueCameraPathCount * 2) {
      if (g_nDragonRescueAudioFiredFlag == 0 &&
          GB[GB_TIMER] >= STATUE_ST(SRC)->speechFrame) {
        CopyWords(save, *(void *volatile *)g_apSampleBankEntriesBlock, 0x14);
        SBE->nSampleAddr = SPU_RAM_SIZE - g_nDragonRescueAudioSampleSize;
        SBE->nLoopAddr = -1;
        SBE->nVolume = RESCUE_VOICE_VOLUME;
        {
          int rate =
              (STATUE_ST(SRCS)->speechRate << 10) / RESCUE_VOICE_BASE_RATE;

          SBE->nPitch = rate;
        }
        SBE->nPitchSpread = 0;
        SBE->nPitchStep = 0;
        SBE->nPitchMode = SAMPLE_PITCH_AROUND;
        ((struct { int v; } *)&g_nSfxOverrideFlags)->v = SFX_OVERRIDE_VOLUME;
        ((struct { short v; } *)&g_nSfxOverrideVolL)->v = SPU_VOLUME_MAX;
        ((struct { short v; } *)&g_nSfxOverrideVolR)->v = SPU_VOLUME_MAX;
        StopSoundVoiceByIndex(GB[GB_VOICE], VOICE_RELEASE);
        PlaySoundEffect(0, 0, SFX_MODE_PANNED, (unsigned char *)&GB[GB_VOICE]);
        CopyWords(*(void *volatile *)g_apSampleBankEntriesBlock, save, 0x14);
        g_nDragonRescueAudioFiredFlag = 1;
      }
      {
        int *p = (int *)((g_nDragonRescueTimer >> 1) * (int)sizeof(CamPathKey) +
                         (int)g_pDragonRescueCameraPath);

        g_anCameraPos[0] = CAM_KEY_RAW(p, x);
        g_anCameraPos[1] = CAM_KEY_RAW(p, y);
        g_anCameraPos[2] = CAM_KEY_RAW(p, z);
        g_nCameraEulerRoll = CAM_KEY_RAW(p, roll);
        g_nCameraEulerPitch = CAM_KEY_RAW(p, pitch);
        g_nCameraEulerYaw = CAM_KEY_RAW(p, yaw);
      }
      FRAME_STEP(g_pDragonRescueStatueModelData, step);
      STATUE->frame = GB[GB_TIMER] / step;
      {
        Actor *p = STATUE;

        p->nextFrame = p->frame + 1;
      }
      STATUE->frameProgress = (GB[GB_TIMER] % step << 6) / step;
      {
        short n =
            ((AnimationHeader *)(FIRST_ANIM(g_pDragonRescueStatueModelData)))
                ->m_NumFrames;
        Actor *p = STATUE;

        if (p->nextFrame >= n) {
          p->frame = n - 1;
          STATUE->frameProgress = 0;
        }
      }
      FRAME_STEP(g_pDragonRescueMirrorModelData, step);
      MIRROR->frame = GB[GB_TIMER] / step;
      {
        Actor *p = MIRROR;

        p->nextFrame = p->frame + 1;
      }
      MIRROR->frameProgress = (GB[GB_TIMER] % step << 6) / step;
      {
        short n =
            ((AnimationHeader *)(FIRST_ANIM(g_pDragonRescueMirrorModelData)))
                ->m_NumFrames;
        Actor *p = MIRROR;

        if (p->nextFrame >= n) {
          p->frame = n - 1;
          MIRROR->frameProgress = 0;
        }
      }
      if (g_dwPadPressed & PAD_CROSS) {
        if (STATUE_ST(SRC)->noSkip != 0) {
          if (g_nDragonRescuePadHoldTimer == 0) {
            g_nDragonRescuePadHoldTimer = 1;
          }
        } else {
          done = 1;
        }
      }
    }
    if (done != 0 ||
        g_nDragonRescueTimer >= g_nDragonRescueCameraPathCount * 2) {
      int *p;

      g_nDragonRescueSubstate = DRAGON_RESCUE_LIGHT_UP;
      g_nDragonRescueTimer = 0;
      p = (int *)(g_nDragonRescueCameraPathCount * (int)sizeof(CamPathKey) +
                  (int)g_pDragonRescueCameraPath);
      g_anCameraPos[0] = CAM_KEY_RAW((CamPathKey *)p - 1, x);
      g_anCameraPos[1] = CAM_KEY_RAW((CamPathKey *)p - 1, y);
      g_anCameraPos[2] = CAM_KEY_RAW((CamPathKey *)p - 1, z);
      g_nCameraEulerRoll = CAM_KEY_RAW((CamPathKey *)p - 1, roll);
      g_nCameraEulerPitch = CAM_KEY_RAW((CamPathKey *)p - 1, pitch);
      g_nCameraEulerYaw = CAM_KEY_RAW((CamPathKey *)p - 1, yaw);
      STATUE->frame =
          ((AnimationHeader *)(FIRST_ANIM(g_pDragonRescueStatueModelData)))
              ->m_NumFrames -
          1;
      STATUE->frameProgress = 0;
      MIRROR->frame =
          ((AnimationHeader *)(FIRST_ANIM(g_pDragonRescueMirrorModelData)))
              ->m_NumFrames -
          1;
      MIRROR->frameProgress = 0;
      g_bSpyroAnimPrev = SPYRO_ANIMID_IDLE;
      g_bSpyroAnimCurrent = SPYRO_ANIMID_IDLE;
      g_bSpyroFramePrev = 0;
      g_bSpyroAnimFrame = 1;
      g_bSpyroSubFrameTimer = 0;
      g_nSpyroAnimPlayRate = 4;
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
      StopSoundVoiceByIndex(*(int *)&g_bSceneSfxVoiceMarker, 2);
      TickSpuPerFrame();
      if (g_nDragonRescueSplitFlag == 0) {
        SetSpuTransferStartAddr(SPU_RAM_SIZE - g_nDragonRescueAudioSampleSize);
        WriteSpuRam(g_pWorldUnpackHead + g_nDragonRescueAudioSampleOffset,
                    g_nDragonRescueAudioSampleSize);
        while (GetSpuTransferStatus(0) == 0) {
        }
      }
    }
    if (GB[GB_PAD_HOLD] != 0) {
      GB[GB_PAD_HOLD] += g_nFrameStep;
      if (GB[GB_PAD_HOLD] >= RESCUE_PAD_HOLD_FRAMES) {
        GB[GB_PAD_HOLD] = 0;
      }
    }
  } break;
  case DRAGON_RESCUE_LIGHT_UP: {
    int *icon;
    int k;
    int off;

    for (i = 0; i < ANIM_HEADER_RAW(FIRST_ANIM(g_pDragonRescueStatueModelData),
                                    short, m_NumColors);
         i++) {
      unsigned int c =
          ComputeDepthCuedColor(((int *)((AnimationHeader *)(FIRST_ANIM(
                                             g_pDragonRescueStatueModelData)))
                                     ->m_Colors)[i],
                                0xFFFFFF, 0x200);
      int *dst = (int *)(i * 4 + ANIM_HEADER_RAW(
                                     FIRST_ANIM(g_pDragonRescueStatueModelData),
                                     int, m_Colors));

      *dst = c;
    }
    AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
    i = 0;
    done = (int)g_anHudRollInStepTable; /* shares s4 with case 4 */
    icon = &HUD_ICON_SLOT(HUD_SLOT_DRAGON_DIGITS)->posY;
    k = 0;
    off = 0;
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2();
  roll_in:
    CopyVector(&HUD_ICON(HUD_SLOT_DRAGON_DIGITS * ACTOR_SIZE + k)->posX,
               (int *)((char *)g_anDragonRescueIconRestPos + off));
    if (g_nDragonRescueTimer < RESCUE_LIGHT_UP_FRAMES) {
      *icon += (((short *)done)[g_nDragonRescueTimer >> 1] * 3) >> 1;
    } else {
      *icon += (g_anHudRollInStepTable[0xC] * 3) >> 1;
    }
    icon += ACTOR_SIZE / 4; /* the next slot's posY */
    k += ACTOR_SIZE;
    i++;
    off += 3 * 4; /* one rest position */
    if (i < HUD_DRAGON_SLOTS) {
      goto roll_in;
    }
    if (GB[GB_TIMER] >= RESCUE_LIGHT_UP_FRAMES) {
      g_nDragonRescueSubstate = DRAGON_RESCUE_COUNT;
      GB[GB_TIMER] = 0;
      g_bHudDragonRollPhase = 0;
      CopyVector((int *)ang, &g_pDragonRescueStatueActor->posX);
      ((int *)ang)[2] += RESCUE_SWIRL_RAISE;
      g_pfnLevelEmitParticles(RESCUE_SWIRL_COUNT, EMIT_RESCUE_SWIRL, ang, 0);
      CopyVector(g_anDragonRescueAuraStarAnchor, (int *)ang);
      DespawnActorRecord(STATUE);
      DespawnActorRecord(MIRROR);
      PlaySoundEffect(LEVEL_SFX(SFX_RESCUE_DEPART), 0, SFX_MODE_PANNED,
                      (unsigned char *)&GB[GB_VOICE]);
    }
  } break;
  case DRAGON_RESCUE_COUNT:
    AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2();
    if (g_anHudDragonCachedCountBlock[0] != g_nTotalDragonsRescued ||
        g_bHudDragonRollPhase != 0) {
      g_bHudDragonRollPhase += RESCUE_COUNT_ROLL_STEP;
      SetHudCounterDigits(HUD_SLOT_DRAGON_DIGITS, 2,
                          g_anHudDragonCachedCountBlock[0] + 1,
                          HUD_DIGITS_PENDING);
      CommitHudCounterAnimState(5, 2, g_bHudDragonRollPhase);
      if (g_bHudDragonRollPhase == HUD_DIGIT_SWAP_PHASE) {
        g_anHudDragonCachedCountBlock[0]++;
      }
      if (g_bHudDragonRollPhase == 0) {
        PlaySoundEffect(LEVEL_SFX(SFX_RESCUE_COUNT), 0, SFX_MODE_PANNED,
                        &g_bSceneSfxVoiceMarker);
      }
    }
    if (GB[GB_TIMER] >= RESCUE_COUNT_FRAMES &&
        g_nDragonRescueStreamState == RESCUE_STREAM_DONE) {
      g_nDragonRescueSubstate = DRAGON_RESCUE_FADE_OUT;
      GB[GB_TIMER] = 0;
      g_pEmitListWriteCursor = g_pEmitListBase;
      ((unsigned char *)g_pEmitListBase)[1] = EMIT_CLASS_END;
      PlaySoundEffect(LEVEL_SFX(SFX_RESCUE_FADE), 0, SFX_MODE_PANNED,
                      (unsigned char *)&GB[GB_VOICE]);
    }
    break;
  case DRAGON_RESCUE_FADE_OUT: {
    int k;

    AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2();
    if (g_nDragonRescueTimer < RESCUE_FADE_FRAMES / 2) {
      int n;
      int *icon;
      int j;
      int off;

      n = 0;
      icon = &HUD_ICON_SLOT(HUD_SLOT_DRAGON_DIGITS)->posY;
      j = 0;
      off = 0;
    roll_out:
      CopyVector(&HUD_ICON(HUD_SLOT_DRAGON_DIGITS * ACTOR_SIZE + j)->posX,
                 (int *)((char *)g_anDragonRescueIconRestPos + off));
      j += ACTOR_SIZE;
      off += 3 * 4; /* one rest position */
      n++;
      *icon += (0x11 - g_nDragonRescueTimer) * 3;
      icon += ACTOR_SIZE / 4; /* the next slot's posY */
      if (n < HUD_DRAGON_SLOTS) {
        goto roll_out;
      }
    }
    if (g_nDragonRescueTimer < RESCUE_FADE_FRAMES / 2) {
      k = g_nDragonRescueTimer;
    } else {
      k = RESCUE_FADE_FRAMES - g_nDragonRescueTimer;
    }
    g_nDragonRescueFullscreenTintLevel = k * 0x10 - 1;
    g_anDragonRescueAuraStarBlock[0] = 1;
    g_bDragonRescueAuraStarColor =
        k * RESCUE_FADE_COLOR_STEP + RESCUE_AURA_COLOR_BASE;
    g_nDragonRescueAuraStarRadius = k * RESCUE_FADE_RADIUS_STEP;
    g_abDragonRescueAuraStarEuler[2] += 8;
    if (g_nDragonRescueTimer >= RESCUE_FADE_FRAMES) {
      g_anDragonRescueAuraStarBlock[0] = 0;
      EndDragonRescueToWorld();
    }
  } break;
  }
  g_pfnLevelAdvanceEmitList(g_nFrameStep);
  if (g_nDragonRescueSubstate > DRAGON_RESCUE_AURA) {
    TickDragonRescueAssetStream();
  }
  {
    Actor *actor = g_pActorListBase;

    while (actor->mode != ACTOR_MODE_END) {
      if (actor->type == ACTOR_TYPE_STATUE_SHARD &&
          actor->mode < ACTOR_MODE_DORMANT) {
        TickStatueBurstShard(actor);
      }
      actor++;
    }
  }
}

#undef GB
#undef GB_TIMER
#undef GB_VOICE
#undef GB_PAD_HOLD
#undef GB_CAM_START_YAW
#undef GB_CAM_TARGET_YAW
#undef GB_STATUE
#undef EUL2
#undef SRC
#undef STATUE
#undef MIRROR
#undef SBE
#undef CAM_KEY_RAW
#undef FIRST_ANIM
#undef W32
#undef H16
#undef U8
#undef REC
#undef STATUE_ST
#undef SRCS
#undef OTY
#undef FRAME_STEP

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
                             ANGLE12_MASK;
        g_nCameraEulerPitch =
            (g_nCameraEulerPitch +
             ((SignedAngleDelta12(ang[1], g_nCameraEulerPitch) *
               g_nSaveMenuAnimPhase) >>
              5)) &
            ANGLE12_MASK;
        g_nCameraEulerYaw = (g_nCameraEulerYaw +
                             ((SignedAngleDelta12(ang[2], g_nCameraEulerYaw) *
                               g_nSaveMenuAnimPhase) >>
                              5)) &
                            ANGLE12_MASK;
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
        SPYB[SPYB_EULER_Z] = ANGLE12_TO_8(g_nSaveMenuSpyroToYaw);
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

#undef SM
#undef SPYB
#undef ACT
#undef LERP_ANGLE

/* Update for GS_ACTOR_CUTSCENE (level-transition title card, 0x800324d8,
   0x548).

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

/* The sky handoff is one SkyHeader struct assignment (lw/lw + sw/sw pairs),
   as in the original, not five scalar copies. */

void ActorCutscene_Update(void) {
  int delta[4];
  SVECTOR aim;
  int settle[4];
  int ease;
  int yaw;
  int radius;
  int goal;
  int start;

  g_anActorCutsceneTimerBlock[ACSW_TIMER] += g_nFrameStep;
  RotateLightVectorXZ(3);

  /* LOAD-BEARING — do not delete, and do not "simplify". The §B-i ebb break:
     this empty loop emits no code, but its NOTE_INSN_LOOP_BEG ends cse's
     extended basic block, so the cse rerun after loop enters the block below
     with no constant equivalence for the g_anActorCutsceneTimerBlock base and
     cannot fold its accesses to absolute. Deleting it costs ~12 instructions
     and the fourth callee-saved register. Header has the pass-by-pass evidence.
   */
  EBB_BREAK();

  if (g_anActorCutsceneTimerBlock[ACSW_SUBSTATE] == ACS_STREAM_WAIT) {
    if (g_nCdStreamState < LSTREAM_RELOCATE) {
      TickLevelTransitionStream(1);
    }

    if (g_anActorCutsceneTimerBlock[ACSW_TIMER] < 0x100) {
      /* Block-scoped per arm on purpose: one function-scope `cam` spanning both
         arms is a single pseudo with a single live range, and it holds a fifth
         callee-saved register the original does not use. */
      int *cam;

      /* 0..0x2000 ease: the sine argument sweeps -0x400..+0x400 across the
         0x100-frame window, so the factor runs a full half-cosine of travel.
         Both shifts are in the source — the sra cannot come from a divide. */
      ease = LookupSine(((g_anActorCutsceneTimerBlock[ACSW_TIMER] << 11) >> 8) -
                        0x400) +
             0x1000;

      /* goal/start are named so both feed the compare AND survive as the call
         arguments: the difference test below leaves them in a0/a1 exactly
         where AbsAngleDelta12 wants them, so the call needs no setup. */
      goal = g_nActorCutsceneCamYawGoal;
      start = g_nActorCutsceneCamYawStart;
      if (ANGLE12_SHORT_WAY_UP(goal, start)) {
        yaw = g_nActorCutsceneCamYawStart +
              ((AbsAngleDelta12(goal, start) * ease) >> 13);
      } else {
        /* Same variable as `start`, but reached through the held base — the
           original re-addresses it here rather than reloading the symbol. */
        yaw = g_anActorCutsceneTimerBlock[ACSW_CAM_YAW_START] -
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

      SubtractVector(delta, &g_pActorCutsceneRecord->posX, cam);
      delta[2] += 0x200;

      if (g_nActorCutsceneTimer < 0x40) {
        aim.vx = 0;
        aim.vy =
            ArcTan2(VectorLength(delta, VEC_LEN_XY), -delta[2], ATAN_ANGLE12);
        aim.vz = ArcTan2(delta[0], delta[1], ATAN_ANGLE12);
        /* Unsigned view for the accumulate only: the call argument above is a
           signed read of the same halfword (lh), the fold back in is not. */
        g_nCameraEulerPitch =
            (*(unsigned short *)&g_nCameraEulerPitch +
             ((SignedAngleDelta12(aim.vy, g_nCameraEulerPitch) *
               g_nActorCutsceneTimer) >>
              6)) &
            ANGLE12_MASK;
        g_nCameraEulerYaw = (*(unsigned short *)&g_nCameraEulerYaw +
                             ((SignedAngleDelta12(aim.vz, g_nCameraEulerYaw) *
                               g_nActorCutsceneTimer) >>
                              6)) &
                            ANGLE12_MASK;
      } else {
        g_nCameraEulerPitch =
            ArcTan2(VectorLength(delta, VEC_LEN_XY), -delta[2], ATAN_ANGLE12);
        g_nCameraEulerYaw = ArcTan2(delta[0], delta[1], ATAN_ANGLE12);
      }
    } else if (g_nCdStreamState == LSTREAM_RELOCATE) {
      int *cam; /* see the approach arm: block-scoped per arm on purpose */
      int face;
      int lvl;

      /* LOAD-BEARING — see the dial before the substate test above. This one
         does the same job for the handoff arm, which is where the base folds
         hardest (13 of the 14). */
      EBB_BREAK();

      *(SkyHeader *)&g_nSkyChunkCount = *(SkyHeader *)&g_nLevelSkyChunkCount;

      g_anActorCutsceneTimerBlock[ACSW_SUBSTATE] = ACS_CRUISE;
      g_anActorCutsceneTimerBlock[ACSW_TIMER] = 0;
      CopyVector(
          &((Actor *)g_anActorCutsceneTimerBlock[ACSW_RECORD])->posX,
          &g_anLevelEntryPoints[g_anActorCutsceneTimerBlock[ACSW_CARD_VARIANT] *
                                4]);

      /* Read in its own statement rather than inline in the index below. This
         is the one read of this field that expand does NOT force_reg — it comes
         out as (set (reg) (mem (const (plus sym 16)))) with the address inline,
         where find_best_addr refuses to touch it, so it can never pick up the
         block base. Naming it puts it back on the force_reg path. Note the
         first read of the SAME field, in the CopyVector call above, is
         force_reg'd fine; spelling this one differently does not help, as
         A[i + 3], (&A[i])[3] and *(A + i + 3) all parse to one tree. */
      lvl = g_anActorCutsceneTimerBlock[ACSW_CARD_VARIANT];
      g_pActorCutsceneRecord->rotZ = (char)g_anLevelEntryPoints[lvl * 4 + 3];
      CopyVector(g_anSpyroWorldPos,
                 &((Actor *)g_anActorCutsceneTimerBlock[ACSW_RECORD])->posX);
      g_nSpyroWorldPosZ += 0x2D4;

      g_abSpyroPersistentEuler[2] = g_pActorCutsceneRecord->rotZ + ANGLE8_HALF;

      /* Written back to the block before the cosine call so the angle is read
         once and reused as the sine argument. */
      g_anActorCutsceneTimerBlock[ACSW_CAM_YAW_GOAL] =
          ((ANGLE8_TO_12(g_pActorCutsceneRecord->rotZ)) + 0x390) & ANGLE12_MASK;

      cam = g_anCameraPosBlock;
      cam[0] = g_pActorCutsceneRecord->posX +
               ((LookupCosine(g_anActorCutsceneTimerBlock[ACSW_CAM_YAW_GOAL]) *
                 g_anActorCutsceneTimerBlock[ACSW_CAM_RADIUS_GOAL]) >>
                12);
      g_anCameraPos[1] =
          g_pActorCutsceneRecord->posY +
          ((LookupSine(g_anActorCutsceneTimerBlock[ACSW_CAM_YAW_GOAL]) *
            g_anActorCutsceneTimerBlock[ACSW_CAM_RADIUS_GOAL]) >>
           12);
      g_anCameraPos[2] = g_pActorCutsceneRecord->posZ + 0x200;

      SubtractVector(settle,
                     &((Actor *)g_anActorCutsceneTimerBlock[ACSW_RECORD])->posX,
                     cam);
      settle[2] += 0x200;

      g_nCameraEulerPitch =
          ArcTan2(VectorLength(settle, VEC_LEN_XY), -settle[2], ATAN_ANGLE12);
      g_nCameraEulerYaw = ArcTan2(settle[0], settle[1], ATAN_ANGLE12);

      /* Named to pin the g_pActorCutsceneRecord load ahead of the g_anCameraPos
         reads below, so the camera load falls into its shadow and the `lbu`
         needs no nop. Left inline in the [13] store, the scheduler hoists
         g_anCameraPos above the g_nCameraEulerYaw store instead and the nop
         costs one. */
      face = g_pActorCutsceneRecord->rotZ;
      g_anActorCutsceneTimerBlock[ACSW_CAM_HEIGHT_GOAL] = g_anCameraPos[2];
      g_anActorCutsceneTimerBlock[ACSW_CAM_HEIGHT_START] =
          g_anCameraPos[2] - 0xFCE;
      g_anActorCutsceneTimerBlock[ACSW_CAM_YAW_START] =
          ((face << 4) + 0xBA0) & ANGLE12_MASK;
    }
  } else {
    g_pfnActorCutsceneSubstateUpdate();
  }

  AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
  TickSpyroAnimLayer1();
  TickSpyroAnimLayer2();
}

/* Poll the pending memcard sync (0x80032a20): MemCardSync(1, &ring[4],
   &ring[5]). When it reports "in progress" (return 1) commit the finished entry
   into the ring slot indexed by g_nMemCardRingIndex, flip that index (1 - it),
   then hand the ring index (<<4) to the card-info op handler RunMemCardInfoOp.
 */

void PollMemCardSyncRing(void) {
  int r = MemCardSync(1, (int *)&g_anMemCardSyncRing[4],
                      (int *)&g_anMemCardSyncRing[5]);
  if (r == 0) {
    return;
  }
  if (r == 1) {
    g_anMemCardSyncRing[g_nMemCardRingIndex] = g_anMemCardSyncRing[5];
    g_nMemCardRingIndex = r - g_nMemCardRingIndex;
  }
  RunMemCardInfoOp(g_nMemCardRingIndex << 4);
}

/* Triangle backs out of the title's memory-card dialogs: return the title
   state block to TITLE_MODE_CARD_CHECK at TITLE_CARD_PROBE step 0 and kick a
   MemCardSync poll whose cmd/result slots sit at +0x40/+0x44 in the same block.
   All three block accesses share one base register. The titlescreen overlay
   tests the return value (v0 still holds the pad test). (0x80032ab0, 88 bytes.)
 */
void RestartTitleCardCheckOnTriangle(void) {
  if (g_dwPadPressed & PAD_TRIANGLE) {
    g_anTitleModeBlock[0] = TITLE_MODE_CARD_CHECK;
    g_nTitleSubState = TITLE_CARD_PROBE;
    g_nStoryIntroSubstate = STORY_INTRO_ARM;
    MemCardSync(0, &g_anTitleModeBlock[0x10], &g_anTitleModeBlock[0x11]);
  }
}

/* Update for GS_TITLE (story-intro / new-game transition, 0x80032b08,
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
  int delta[4];
  RECT clear;

  if (g_nTitleMode == TITLE_MODE_STORY_INTRO) {
    if (g_nStoryIntroSubstate == STORY_INTRO_ARM) {
      StopAllSoundExceptMask(0);
      TickSpuPerFrame();
      g_nStoryIntroSubstate = STORY_INTRO_STREAM;
      if (g_nTitleExitPath == TITLE_EXIT_NEW_GAME) {
        g_nCutsceneId = CUTSCENE_OPENING;
        g_nCdStreamState = CSTREAM_READ_HEADER;
        g_pLoadStagingBuffer = g_pSharedLoadBuffer;
        g_pLevelDataBuffer = g_pSharedLoadBuffer;
      } else if (g_nTitleExitPath == TITLE_EXIT_LOADED_GAME) {
        g_nFlightLevelActive = 0;
        g_nCdStreamState = LSTREAM_FRESH;
        g_nReturnPortalLevelId = 0;
        g_nPortalTransitionActive = 0;
        g_nNextLevelId = g_nActiveLevelId;
      } else if (g_nTitleExitPath == TITLE_EXIT_DEMO) {
        InitNewGameState();
        g_nFlightLevelActive = 0;
        g_nCdStreamState = LSTREAM_FRESH;
        g_nReturnPortalLevelId = 0;
        g_nPortalTransitionActive = 0;
        g_nActiveLevelId = g_anDemoLevelIds[g_nDemoIndex];
        g_nNextLevelId = g_nActiveLevelId;
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
      g_bSpyroAnimPrev = SPYRO_ANIMID_GLIDE;
      g_bSpyroAnimCurrent = SPYRO_ANIMID_GLIDE;
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

      CopyVector(delta, pos);
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

      SubtractVector(delta, g_anSpyroWorldPos, delta);
      g_abSpyroPersistentEuler[1] =
          ArcTan2(VectorLength(delta, VEC_LEN_XY), delta[2], ATAN_ANGLE8);
      g_abSpyroPersistentEuler[2] = ArcTan2(delta[0], delta[1], ATAN_ANGLE8);
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
            clear.w = SCREEN_W;
            clear.h = SCREEN_H * 2;
            ClearImage(&clear, 0, 0, 0);
            DrawSync(0);
            g_pWorkAreaTop = (void *)(RAM_END - g_nCrt0StackSize);
            InitActorMeshScratchRegions(1);
            while (g_nCdStreamState < CSTREAM_DONE) {
              TickCutsceneBundleLoadStream();
              TickCdMusicStream();
            }
            BeginCutscene();
            g_nSkipGamestateDraw = 1;
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

/* 0x800331ac — GS_CUTSCENE (0xE) per-tick handler: on first entry (timer
   0, hub world) clear the screen, stream the cutscene bundle in synchronously
   and re-seed via BeginCutscene; per tick advance the cutscene timer toward
   twice the threshold, fade g_nScreenFadeLevel at both ends, allow a pad
   press to skip ahead, then hand off to DispatchSpecialCutscene at the end. */
void Cutscene_Update(void) {
  RECT rect;
  CutscenePath *path;
  int t;
  int end;
  CutscenePath *p;
  int cur;

  if (g_nCutsceneId >= 2 && g_pPathTableHead->frame == 0) {
    StopAllSoundExceptMask(0);
    TickSpuPerFrame();
    rect.w = SCREEN_W;
    rect.x = 0;
    rect.y = 0;
    rect.h = SCREEN_H * 2;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    g_nCdStreamState = CSTREAM_READ_HEADER;
    g_pLoadStagingBuffer = g_pSharedLoadBuffer;
    g_pLevelDataBuffer = g_pSharedLoadBuffer;
    if (g_nCutsceneId == CUTSCENE_LOOT_ENDING) {
      g_pWorkAreaTop = (void *)(RAM_END - g_nCrt0StackSize);
      InitActorMeshScratchRegions(1);
    }
    while (g_nCdStreamState < CSTREAM_DONE) {
      TickCutsceneBundleLoadStream();
      TickCdMusicStream();
    }
    BeginCutscene();
  }

  path = g_pPathTableHead;
  t = path->frame + g_nFrameStep;
  end = path->length << 1;
  path->frame = t;
  if (t < end) {
    if (t < 0x10 || (t = end - t, t < 0x10)) {
      g_nScreenFadeLevel = 0x10 - t;
    } else {
      g_nScreenFadeLevel = 0;
    }
    if (g_nScreenFadeLevel >= 0x10) {
      g_nScreenFadeLevel = 0xF;
    }
    if (g_nCutsceneId == CUTSCENE_OPENING &&
        (g_dwPadHeld & (PAD_CROSS | PAD_START))) {
      p = g_pPathTableHead;
      cur = p->frame;
      if (cur >= 0xF1 && cur < (p->length << 1) - 0x20) {
        p->length = (cur >> 1) + 0x10;
      }
    }
    AdvanceCutsceneFrame(g_pPathTableHead->frame);
  } else {
    DispatchSpecialCutscene();
  }
}

/* GS_CREDITS updater (0x800333dc) — the credits/return-home transition.
   Spins the light vector each frame. Mode 0x63 arms the CD stream and picks
   the destination level: a pending credits-return level if one is latched,
   otherwise Gnasty's Loot (0x3C), or Dream Weavers home (0xA) once every
   treasure (0x36B0 gems) is collected; then clears the scripted-cutscene
   state and advances to mode 0x64, which streams the level transition. */

void CreditsSequence_Update(void) {
  RotateLightVectorXZ(3);
  switch (g_nCreditsMode) {
  case CREDITS_MODE_RETURN_HOME:
    g_nCdStreamState = LSTREAM_FRESH;
    if (g_nCreditsReturnLevelId != -1) {
      g_nNextLevelId = g_nCreditsReturnLevelId;
      g_nCreditsReturnLevelId = -1;
    } else {
      if (g_nTotalTreasureCollected == 0x36B0) {
        g_nNextLevelId = LEVEL_ARTISANS;
      } else {
        g_nNextLevelId = LEVEL_GNORC_GNEXUS;
      }
    }
    g_nReturnPortalLevelId = 0;
    g_nPortalTransitionActive = 0;
    g_nReturningHomePhase = 0;
    g_nCreditsMode += 1;
    break;
  case CREDITS_MODE_RETURN_HOME_STREAM:
    g_nReturningHomePhase += g_nFrameStep;
    TickLevelTransitionStream(1);
    break;
  }
}

/* The sample-bank stores go through the reloaded global pointer each field
   (the original never proves it points at &g_CutsceneSoundDef); a volatile read
   of the pointer defeats gcc's constant-propagation of the just-assigned
   address. */
#define BANK (*(char *volatile *)&g_pLevelSampleBankEntries)

/* Attract-demo watcher (gameplay path, g_nDemoMode != DEMO_OFF). Plays back the
   recorded pad; once the demo runs out (or a real button is pressed) it ramps
   the fade, and at 16 tears down the level, sync-loads cutscene bundle 0 (the
   title backdrop), enters GS_CUTSCENE and advances g_nDemoIndex. Returns 1
   when the demo ended this frame, else 0.
   (0x800334d4, 904 bytes.) */
int TickDemoPlayback(void) {
  RECT rect[2]; /* only rect[0] is used; a single RECT shrinks the frame by 8 */

  SampleDemoPadFrame();
  if (g_nDemoMode != DEMO_RECORD) {
    if (g_nDemoExitFadeFrame == 0) {
      if (g_anDemoLengthFrames[g_nDemoIndex] - 8 <= (int)g_dwGamestateFrames ||
          ((int)g_dwGamestateFrames > 0xf && g_abPadRawReport[0] == 0 &&
           (g_abPadRawReport[2] != 0xff || g_abPadRawReport[3] != 0xff))) {
        g_nDemoExitFadeFrame = 1;
      }
    } else {
      g_nDemoExitFadeFrame = g_nDemoExitFadeFrame + 1;
      g_nScreenFadeLevel = g_nDemoExitFadeFrame * 2;
    }
    if (g_nDemoExitFadeFrame == 0x10) {
      StopAllSoundExceptMask(0);
      TickSpuPerFrame();
      rect[0].w = SCREEN_W;
      rect[0].x = 0;
      rect[0].y = 0;
      rect[0].h = SCREEN_H * 2;
      ClearImage(rect, 0, 0, 0);
      DrawSync(0);
      g_pLoadStagingBuffer = &g_abTitlescreenOverlayEnd;
      g_pLevelDataBuffer = &g_abTitlescreenOverlayEnd;
      g_nHitRumbleTimer = 0;
      g_nVibrationLevel = 0;
      g_nPulseRumbleTimer = 0;
      g_nPulseRumbleAmount = 0;
      g_nDemoMode = DEMO_OFF;
      g_nCdStreamState = CSTREAM_READ_HEADER;
      g_nCutsceneId = CUTSCENE_TITLE_BACKDROP;
      g_nFlightLevelActive = 0;
      g_nDemoActive = 0;
      {
        int lba = *(volatile int *)&g_nCdBaseLba;
        int count = *(volatile int *)&g_nLevelLoadChunkSize;
        int offset = *(volatile int *)&g_nLevelLoadByteOffset;
        void *dst = *(void *volatile *)&g_pSharedLoadBuffer;
        CdReadSyncSectors(lba, dst, count, offset, CD_STALL_TIMEOUT_TICKS);
      }
      while (g_nCdStreamState < CSTREAM_DONE) {
        TickCutsceneBundleLoadStream();
        TickCdMusicStream();
      }
      g_nGamestate = GS_CUTSCENE;
      g_nScreenFadeLevel = 0xf;
      BeginStoryIntro();
      if ((int)g_dwGamestateFrames >= g_anDemoLengthFrames[g_nDemoIndex]) {
        /* CutscenePath.frame as a plain int store: the member store
           reschedules the SPU attribute block below (A400) */
        *(int *)g_pPathTableHead = 0x2d0;
        g_pLevelSampleBankEntries = g_CutsceneSoundDef;
        SAMPLE_RAW(BANK, int, nSampleAddr) = 0x262d0;
        SAMPLE_RAW(BANK, int, nLoopAddr) = -1;
        SAMPLE_RAW(BANK, short, nVolume) = 0x50;
        SAMPLE_RAW(BANK, short, nPitch) = g_nCutsceneSamplePitch;
        SAMPLE_RAW(BANK, short, nPitchSpread) = 0;
        SAMPLE_RAW(BANK, short, nPitchStep) = 0;
        SAMPLE_RAW(BANK, int, nPitchMode) = SAMPLE_PITCH_AROUND;
        g_nSfxOverrideFlags = SFX_OVERRIDE_VOLUME;
        g_nSfxOverrideVolL = SPU_VOLUME_MAX;
        g_nSfxOverrideVolR = SPU_VOLUME_MAX;
        PlaySoundEffect(0, 0, SFX_MODE_PANNED, &g_bSceneSfxVoiceMarker);
      } else {
        *(int *)g_pPathTableHead = 0x490;
      }
      g_nTitleSubState = 1;
      {
        int lba = *(volatile int *)&g_nCdBaseLba;
        int offset = *(volatile int *)&g_nTitleSceneSectorOffset;
        int dst = -TIM_PAGE_BYTES +
                  (int)*(void *volatile *)&g_pRenderScratchRegionBase;
        StartCdReadAsync(lba, (void *)dst, TIM_PAGE_BYTES, offset,
                         CD_STALL_TIMEOUT_TICKS);
      }
      g_nSkipGamestateDraw = 1;
      g_nDemoIndex = (g_nDemoIndex + 1) & 3;
      return 1;
    }
  }
  return 0;
}

#undef BANK

/* Per-frame top-level update dispatcher (0x8003385C, 0x3f4).

   Runs the audio ticks, then dispatches the active gamestate's Update handler.
   Two of the arms hand off to the overlay resident at 0x8007xxxx: state 0xD
   runs the titlescreen entry unless its sub-mode reached 3, and state 0xF runs
   the credits entry until its sub-mode passes 0x63.

   State 0 is the gameplay frame itself: age the frame counter, bail out early
   when the death-plane check fires, run the world-chunk animations and the two
   level hooks around the Spyro frame tick, then the HUD/camera work. Both
   halves of the Spyro tick re-read g_nGamestate, since any of the handlers can
   switch state mid-frame. Finally, while genuinely in-level and unblocked, the
   Start/Select buttons open the pause and inventory menus (a disconnected or
   unknown pad type forces the pause menu). Every path ends in the SPU tick
   except the early returns. */

void GamestateUpdate(void) {
  g_nSkipGamestateDraw = 0;
  TickActiveSoundVoices();
  TickCdMusicStream();

  if (g_nGamestate != GS_GAMEPLAY) {
    if (g_nGamestate == GS_LEVEL_LOAD) {
      LevelLoad_Update();
    } else if (g_nGamestate == GS_PAUSE_MENU) {
      PauseMenu_Update();
    } else if (g_nGamestate == GS_INVENTORY) {
      SubMenu_Update();
    } else if (g_nGamestate == GS_RESPAWN) {
      RespawnOrGameOver_Update();
    } else if (g_nGamestate == GS_GAME_OVER) {
      RespawnOrGameOver_Update();
    } else if (g_nGamestate == GS_UNUSED_6) {
      Gamestate06_Update();
    } else if (g_nGamestate == GS_FLIGHT_RESULTS) {
      g_pfnFlightResultsUpdate();
    } else if (g_nGamestate == GS_DRAGON_RESCUE) {
      DragonRescue_Update();
    } else if (g_nGamestate == GS_ARRIVAL_CAMERA) {
      CameraSettle_Update();
    } else if (g_nGamestate == GS_MENU_TRANSITION) {
      SpyroStateDelay_Update();
    } else if (g_nGamestate == GS_SAVE_MENU) {
      SaveMenu_Update();
    } else if (g_nGamestate == GS_ACTOR_CUTSCENE) {
      ActorCutscene_Update();
    } else if (g_nGamestate == GS_TITLE) {
      if (g_nTitleMode != TITLE_MODE_STORY_INTRO) {
        Titlescreen_Update();
      } else {
        StoryIntro_Update();
      }
    } else if (g_nGamestate == GS_CUTSCENE) {
      Cutscene_Update();
    } else if (g_nGamestate == GS_CREDITS) {
      if (g_nCreditsMode >= CREDITS_MODE_RETURN_HOME) {
        CreditsSequence_Update();
      } else {
        Credits_Update();
      }
    }
  } else {
    g_dwGamestateFrames += 1;
    if (g_nDemoMode != DEMO_OFF) {
      if (TickDemoPlayback()) {
        return;
      }
    }

    TickWorldChunkAnimations(g_nFrameStep);
    g_pfnLevelUpdateActors();

    if (g_nGamestate == GS_SAVE_MENU || g_nGamestate == GS_ACTOR_CUTSCENE ||
        g_nGamestate == GS_FLIGHT_RESULTS || g_nGamestate == GS_LEVEL_LOAD) {
      return;
    }
    TickSpyroGameplayFrame();
    if (g_nGamestate == GS_RESPAWN || g_nGamestate == GS_GAME_OVER ||
        g_nGamestate == GS_FLIGHT_RESULTS || g_nGamestate == GS_LEVEL_LOAD) {
      return;
    }

    g_pfnLevelAdvanceEmitList(g_nFrameStep);
    if (!g_nFlightLevelActive) {
      UpdateHudCounters();
    }
    ComputeBillboardBasisVectors();
    UpdateCameraFrame();

    if (g_nScreenFadeLevel != 0) {
      g_nScreenFadeLevel -= g_nFrameStep;
      if (g_nScreenFadeLevel < 0) {
        g_nScreenFadeLevel = 0;
      }
    }

    if (g_nGamestate == GS_GAMEPLAY && g_nCinematicActive == 0 &&
        g_nSparxHealth >= SPARX_GONE &&
        g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE)) {
      if ((unsigned int)g_nPadType < PAD_TYPE_DIGITAL ||
          (g_dwPadPressed & PAD_START)) {
        EnterPauseMenu(1);
      } else if (g_dwPadPressed & PAD_SELECT) {
        EnterInventoryMenu(1);
      }
    }
  }

  TickSpuPerFrame();
}
