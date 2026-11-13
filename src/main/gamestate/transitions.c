#include "actor_states.h"
#include "actor_type.h"
#include "angle.h"
#include "camera.h"
#include "cd_music.h"
#include "cd_stream.h"
#include "credits_mode.h"
#include "cutscene_id.h"
#include "cutscene_path.h"
#include "dragon_rescue.h"
#include "frame_db.h"
#include "funcs.h"
#include "globals.h"
#include "gpu_prim.h"
#include "hud_counter.h"
#include "level_id.h"
#include "level_sfx.h"
#include "libcd.h"
#include "main_shared.h"
#include "match_asm.h"
#include "model.h"
#include "pause_menu.h"
#include "sample_bank.h"
#include "save_menu.h"
#include "spu.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "title_state.h"
#include "vecmath.h"

/* Brings the console hardware subsystems back to a known idle state: blanks the
   display, re-inits the CD, clears the CD read callback, the VSync callback and
   pad comms, resets the GPU, then runs the libapi reset helper.
   (0x8002ba68, 80 bytes.) */
void QuiesceHardwareForExec(void) {
  SetDispMask(0);
  CdInit();
  CdReadCallback(0);
  InstallVSyncCallback(0);
  PadStopCom();
  ResetGraph(3);
  CallLibapiSysSlot10();
}

/* Tears the running game down and chain-loads the crash/demo executable: logs
   "Shutting Down", quiesces the hardware (QuiesceHardwareForExec), deinits and
   re-inits the BIOS 96 CD-FS, logs "Launching Crash Demo", then LoadExec's
   CRASH.EXE. (0x8002bab8, 104 bytes.) */
void LaunchCrashDemo(void) {
  WritePrintf(g_szShuttingDownMsg);
  QuiesceHardwareForExec();
  Bios96Remove();
  _96_init();
  WritePrintf(g_szLaunchingCrashDemoMsg);
  LoadExec(g_szCrashDemoExePath, RAM_END - 0x100, 0);
}

/* Inventory completion percentage (0x8002bb20): gems are 50%, dragons 40%,
   eggs 10% — gems*50 + dragons*6000 + eggs*10000, out of 12000*100. Past the
   12000-gem cap (100%+ via cheats) it counts single percent per 100 extra
   gems above 0x2EE0. */
int ComputeInventoryProgressPercent(void) {
  int pct = (g_nTotalTreasureCollected * 50 + g_nTotalDragonsRescued * 6000 +
             g_nTotalEggsRecovered * 10000) /
            12000;
  if (g_nTotalTreasureCollected >= 0x2EE1) {
    pct = (g_nTotalTreasureCollected - 0x2EE0) / 100 + 100;
  }
  return pct;
}

/* The CdlLOC bytes at 0x800776c1 reached from the music-stream block's
   held base, as the original addresses it. */
#define CD_MUSIC_POS ((unsigned char *)g_adwMusicStreamBlock + 0x20D)

/* Levers (all load-bearing):
     - frame 0x38 -> 0x30: A231. The CdSync==5 arm's two LBA-table reads
       straddle CdIntToPos/CdControl; with one base symbol cse makes them one
       pseudo live across the calls and reload buys it an unreferenced 8-byte
       slot. The post-call read uses g_anMusicTrackLbaTablePost.
     - old RESIDUE 1 (the `li 0x1B; sw g_nLastCdMusicCommand` triple filling
       the track-load delay slot): read the track into its own local `trk`
       AFTER CdControl, then store the command through a one-member struct
       view (A230: an in-struct store keeps the in-struct array read of the
       table below it).
   Earlier levers still load-bearing: the end-LBA half has its own symbol
   (g_anMusicTrackEndLbaTable); `if (g_nVblankTickCount >
   g_nCdMusicStreamTimeout)` (A120 load order); the fade tail's held base +
   empty do/while(0) (A200) and its per-arm "store the new volume" blocks
   (A201).
   - the random-track table base is a block-local pointer (`base`): cse keeps
     the cheaper register, so the `la` is emitted with the index load and
     fills its load-delay bubble, as in the original. */

/* Views off the held fade-target base (g_nMusicVolumeFadeTarget). */
#define FADE_TARGET (g_anMusicFadeBlock[0])
#define FADE_STEP (g_anMusicFadeBlock[0x16]) /* g_nMusicVolumeFadeStep */
#define CD_DA_VOLUME                                                           \
  (*(short *)&g_anMusicFadeBlock[-0xB0]) /* g_nCdDaVolumeCurrent */

/* 0x8002bbe0 (1024 bytes) — per-frame CD-DA music stream tick plus the volume
   fade ramp.  Polls the in-flight CD command, retires it (or dispatches a
   queued one), keeps the current track streaming — advancing to a random track
   from the world's triple once the read head passes the track end, or
   re-syncing the read head from the drive while still inside it — and finally
   steps the SPU master volume one frame toward its fade target. */
void TickCdMusicStream(void) {
  char pos[8];
  char filt[8];
  int sync;
  int cmd;
  int target;
  int vol;
  int step;
  int r;
  int trk;
  int pick;
  int *tbl;

  sync = CallCdSync(1, g_abCdSyncResult);
  cmd = g_nLastCdMusicCommand;
  if (cmd != 0) {
    switch ((unsigned int)cmd) {
    case CdlStop:
    case CdlPause:
      if ((g_dwMusicStreamStatus & MUSIC_ST_FADING) == 0 && sync == 2) {
        g_dwMusicStreamStatus = MUSIC_ST_IDLE;
        g_nLastCdMusicCommand = 0;
      }
      break;
    case CdlReadS:
      g_dwMusicStreamStatus = MUSIC_ST_PLAYING;
      g_nLastCdMusicCommand = 0;
      break;
    }
  } else if (g_nPendingMusicCommand != 0) {
    HandleMusicCommand(g_nCurrentMusicTrack, g_nPendingMusicCommand);
    g_nPendingMusicCommand = 0;
  }

  if (g_adwMusicStreamBlock[0] & MUSIC_ST_PLAYING) {
    if (sync != 2) {
      if (sync == 5) {
        CdIntToPos(g_anMusicTrackLbaTable[g_nCurrentMusicTrack * 2], pos);
        CdControl(CdlReadS, pos, 0);
        trk = g_nCurrentMusicTrack;
        ((struct { int v; } *)&g_nLastCdMusicCommand)->v = CdlReadS;
        g_dwCdMusicReadHead = g_anMusicTrackLbaTablePost[trk * 2];
      }
    } else {
      if ((unsigned int)g_anMusicTrackEndLbaTable[g_nCurrentMusicTrack * 2] <
          g_dwCdMusicReadHead) {
        if (g_nVblankTickCount > g_nCdMusicStreamTimeout) {
          pick = (int)GetRandomU32();
          {
            int *base = g_anLevelMusicPickTable;
            tbl = &base[g_nLevelIndex * 3];
          }
          g_nCdMusicStreamTimeout = g_nVblankTickCount + MUSIC_REPICK_TICKS;
          g_nCurrentMusicTrack = (&g_nLevelMusicTrack)[tbl[pick % 3]];
        }
        g_dwCdMusicReadHead = g_anMusicTrackLbaTable[g_nCurrentMusicTrack * 2];
        CdIntToPos(g_dwCdMusicReadHead, pos);
        filt[0] = 1;
        filt[1] = g_nCurrentMusicTrack & 7;
        CdControlBlocking(CdlSetfilter, filt, 0);
        CdControl(CdlReadS, pos, 0);
      } else {
        if (GetCdStatusByte() == 0x11) {
          r = CdPosToInt(CD_MUSIC_POS);
          if (r > 0) {
            g_dwCdMusicReadHead = r;
          }
        }
        CdControlForce(CdlGetlocP, 0);
      }
    }
  }

  target = FADE_TARGET;
  if (target >= 0) {
    EBB_BREAK();
    step = FADE_STEP;
    vol = CD_DA_VOLUME;
    vol = vol + step;
    if (step < 0) {
      if (target < vol) {
        CD_DA_VOLUME = vol;
        goto store_right;
      }
      if (target == 0) {
        switch ((unsigned int)g_nLastCdMusicCommand) {
        case CdlStop:
        case CdlPause:
          CdControl(g_nLastCdMusicCommand, 0, 0);
          g_dwMusicStreamStatus = MUSIC_ST_PAUSED;
          break;
        case CdlReadS:
          g_dwMusicStreamStatus = MUSIC_ST_PLAYING;
          break;
        }
      }
      vol = g_nMusicVolumeFadeTarget;
      g_nMusicVolumeFadeStep = 0;
      g_nMusicVolumeFadeTarget = -1;
      g_nCdDaVolumeCurrent = vol;
      goto store_right;
    }
    if (vol < target) {
      CD_DA_VOLUME = vol;
      goto store_right;
    }
    FADE_STEP = 0;
    CD_DA_VOLUME = target;
    g_nSpuCommonCdVolumeR = target;
    FADE_TARGET = -1;
    goto spu;
  store_right:
    g_nSpuCommonCdVolumeR = vol;
  spu:
    *(unsigned int *)g_abSpuCommonAttr = SPU_COMMON_CDVOL;
    SetSpuCommonAttr(g_abSpuCommonAttr);
  } else {
    if (g_adwMusicStreamBlock[0] & MUSIC_ST_FADING) {
      g_adwMusicStreamBlock[0] = MUSIC_ST_PAUSED;
    }
  }
}

#undef CD_MUSIC_POS
#undef FADE_TARGET
#undef FADE_STEP
#undef CD_DA_VOLUME

/* The camera euler trio is written as one short[3]
   (g_anCameraEulerRollBlock: array store, so the loop guard load stays below
   it — globals.h types the three as scalars). */

/* 0x8002bfe0 — per-tick scripted-cutscene pose update (1088 b, leaf). Reads the
   current path node (timer>>1, stride 0x18) into the camera pos/euler
   globals, then for each cutscene actor derives a segment index + 1/64
   fraction from the progress rate of its model's first animation
   (m_ProgressPerTick ANIM_RATE_TICKS(n) holds a keyframe 2n timer ticks:
   0x10 /8, 0x15 /6, 0x20 /4, else /2), writes the
   seg/60 keyframe split into the actor record and lerps the packed 10-bit
   path points into the actor position. */
void AdvanceCutsceneFrame(int timer) {
  CutscenePath *path;
  CutscenePath *hdr;
  int i;
  int seg;
  int frac;
  int inv;
  Actor *ap;
  int a[3];
  int b[3];
  int pad[2];
  unsigned char rate;

  path = g_pPathTableHead;
  g_anCameraPos[0] = path->camera[(path->frame >> 1) * 6];
  g_anCameraPos[1] = path->camera[(path->frame >> 1) * 6 + 1];
  g_anCameraPos[2] = path->camera[(path->frame >> 1) * 6 + 2];
  g_anCameraEulerRollBlock[0] = path->camera[(path->frame >> 1) * 6 + 3];
  g_anCameraEulerRollBlock[1] = path->camera[(path->frame >> 1) * 6 + 4];
  g_anCameraEulerRollBlock[2] = path->camera[(path->frame >> 1) * 6 + 5];

  i = 0;
  if (path->actorCount > 0) {
    do {
      rate = ((Model *)g_apActorMeshTable_1[i])
                 ->m_Animations[0]
                 ->m_ProgressPerTick;
      if (rate == ANIM_RATE_TICKS(4)) {
        int t = g_pPathTableHead->frame;
        seg = t >> 3;
        frac = (t & 7) << 3;
      } else if (rate == ANIM_RATE_TICKS(3)) {
        int t = g_pPathTableHead->frame;
        seg = t / 6;
        frac = (t % 6) * 10;
      } else if (rate == ANIM_RATE_TICKS(2)) {
        int t = g_pPathTableHead->frame;
        seg = t >> 2;
        frac = (t & 3) << 4;
      } else {
        int t = g_pPathTableHead->frame;
        seg = t >> 1;
        frac = (t & 1) << 5;
      }
      g_pActorListBase[i].anim = seg / CUTSCENE_ANIM_FRAMES;
      g_pActorListBase[i].frame = seg % CUTSCENE_ANIM_FRAMES;
      g_pActorListBase[i].nextAnim = (seg + 1) / CUTSCENE_ANIM_FRAMES;
      g_pActorListBase[i].nextFrame = (seg + 1) % CUTSCENE_ANIM_FRAMES;
      g_pActorListBase[i].frameProgress = frac;
      hdr = g_pPathTableHead;
      a[0] = (hdr->track[i][seg] & 0x3FF00000) >> 12;
      a[1] = (hdr->track[i][seg] & 0xFFC00) >> 2;
      a[2] = (hdr->track[i][seg] & 0x3FF) << 8;
      b[0] = (hdr->track[i][seg + 1] & 0x3FF00000) >> 12;
      b[1] = (hdr->track[i][seg + 1] & 0xFFC00) >> 2;
      b[2] = (hdr->track[i][seg + 1] & 0x3FF) << 8;
      inv = ANIM_STEP - frac;
      ap = g_pActorListBase;
      ap[i].posX = (a[0] * inv + b[0] * frac) >> 6;
      ap[i].posY = (a[1] * inv + b[1] * frac) >> 6;
      ap[i].posZ = (a[2] * inv + b[2] * frac) >> 6;
      i++;
    } while (i < hdr->actorCount);
  }
}

/* Enter the pause menu gamestate (0x8002c420). param nonzero = paused from
   gameplay (stop SFX, reset the transition frames). Plays the menu-open sound,
   switches to GS_PAUSE_MENU and zeros the pause-menu cursor/substate/idle
   timer. Flight levels skip all HUD work; otherwise re-init the HUD counters,
   and on the param==0 re-entry path force all five counters visible with roll
   phase 0xc, then refresh the counters. */

void EnterPauseMenu(int fromGameplay) {
  if (fromGameplay != 0) {
    StopAllSoundExceptMask(0);
  }
  PlaySoundEffect(LEVEL_SFX(SFX_MENU_OPEN), 0, SFX_MODE_PANNED, 0);
  g_nGamestate = GS_PAUSE_MENU;
  g_nPauseMenuCursor = PAUSE_ROW_CONTINUE;
  g_nPauseMenuSubstate = PAUSE_PAGE_MAIN;
  g_nMenuTickFrames = 0;
  if (g_nFlightLevelActive != 0) {
    if (fromGameplay != 0) {
      g_nMenuDrawFrames = 0;
    }
    return;
  }
  InitHudCounters(0);
  if (fromGameplay != 0) {
    g_nMenuDrawFrames = 0;
  } else {
    g_abHudCounterDisplayState[HUD_CTR_GEMS] = HUD_CTR_ROLL_IN;
    g_abHudCounterDisplayState[HUD_CTR_DRAGONS] = HUD_CTR_ROLL_IN;
    g_abHudCounterDisplayState[HUD_CTR_LIVES] = HUD_CTR_ROLL_IN;
    g_abHudCounterDisplayState[HUD_CTR_EGGS] = HUD_CTR_ROLL_IN;
    g_abHudCounterDisplayState[HUD_CTR_KEY] = HUD_CTR_ROLL_IN;
    g_bHudLevelGemRollPhase = 0xC;
    g_bHudDragonRollPhase = 0xC;
    g_bHudLivesRollPhase = 0xC;
    g_bHudEggRollPhase = 0xC;
    g_bHudKeyRollPhase = 0xC;
  }
  UpdateHudCounters();
}

/* Leave the pause menu back to gameplay (0x8002c534). Blit the saved 0x200x0xE1
   backdrop tile from render scratch back into the frame buffer, wait for the
   transfer, return to GS_GAMEPLAY and re-init lighting, then force all five HUD
   counters visible with roll phase 0xd. When leaving from gameplay (param != 0)
   resume the current music track. */

void ExitPauseMenuToGame(int resumeMusic) {
  RECT rect;

  rect.x = 0x200;
  rect.w = 0x100;
  rect.y = 0;
  rect.h = 0xE1;
  LoadImage(&rect, (char *)g_pRenderScratchPrimTop - MENU_SNAPSHOT_BYTES);
  DrawSync(0);
  g_nGamestate = GS_GAMEPLAY;
  InitLightVectorConstants();
  g_abHudCounterDisplayState[HUD_CTR_GEMS] = HUD_CTR_ROLL_OUT;
  g_abHudCounterDisplayState[HUD_CTR_DRAGONS] = HUD_CTR_ROLL_OUT;
  g_abHudCounterDisplayState[HUD_CTR_LIVES] = HUD_CTR_ROLL_OUT;
  g_abHudCounterDisplayState[HUD_CTR_EGGS] = HUD_CTR_ROLL_OUT;
  g_abHudCounterDisplayState[HUD_CTR_KEY] = HUD_CTR_ROLL_OUT;
  g_bHudLevelGemRollPhase = 0xD;
  g_bHudDragonRollPhase = 0xD;
  g_bHudLivesRollPhase = 0xD;
  g_bHudEggRollPhase = 0xD;
  g_bHudKeyRollPhase = 0xD;
  if (resumeMusic != 0) {
    HandleMusicCommand(g_nCurrentMusicTrack, MUSIC_CMD_RESUME);
  }
}

/* Leave gameplay for the pause/menu gamestate (0xA): kill all sound, reset the
   pause-menu frame timers, and park Spyro in his idle state. */
void BeginPauseToMenuTransition(void) {
  StopAllSoundExceptMask(0);
  g_nGamestate = GS_MENU_TRANSITION;
  g_nMenuTickFrames = 0;
  g_nMenuDrawFrames = 0;
  ResetSpyroEntity(RESET_SPYRO_SOFT);
  ChangeSpyroState(SPYRO_ST_IDLE);
}

/* Leave a level for its world's home (0x8002c664): the hub id is the level
   id rounded down to a multiple of 10. Starts the GS_LEVEL_LOAD stream in its
   preserve phase with the level-transition camera, remembers the level as the
   return portal (g_nReturnPortalLevelId) and pauses the game while it loads. */
void BeginWorldHubReturn(void) {
  int hub;
  InitLightVectorConstants();
  hub = (g_nActiveLevelId / 10) * 10;
  g_nCdStreamState = LSTREAM_PRESERVE;
  g_nLevelTransitionPhase = 0;
  g_nCameraNextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
  g_nEnteredPortalIdx = -1;
  g_nReturnPortalLevelId = g_nActiveLevelId;
  g_nGamestate = GS_LEVEL_LOAD;
  g_nPortalTransitionActive = 1;
  g_nLevelTransitionTallyActive = 1;
  g_nSkipGamestateDraw = 1;
  g_nNextLevelId = hub;
}

/* Enter the inventory / sub-menu gamestate (0x8002c714). param nonzero = opened
   from gameplay (stop SFX first, reset the pause transition frames). Plays the
   menu-open sound (sample id at g_pLevelSampleBankHeader+0x2c), zeros the
   sub-menu scroll state, switches to GS_INVENTORY, seats the cursor on the
   current world (g_nLevelIndex / 6), then clears the cheat-code pad-history
   ring. */

void EnterInventoryMenu(int fromGameplay) {
  if (fromGameplay != 0) {
    StopAllSoundExceptMask(0);
  }
  PlaySoundEffect(LEVEL_SFX(SFX_MENU_OPEN), 0, SFX_MODE_PANNED, 0);
  g_nSubMenuScrollOffset = 0;
  g_nSubMenuScrollDelta = 0;
  g_nGamestate = GS_INVENTORY;
  g_nSubMenuCursor = g_nLevelIndex / 6;
  if (fromGameplay != 0) {
    g_nMenuDrawFrames = 0;
  }
  ClearPadHistoryRing();
}

/* Tear down a sub-menu and return to the title: blit the saved framebuffer
   region back from VRAM scratch, reset gamestate to 0, re-prime lighting, clear
   the HUD counter display flags, and restart the title music (0x8002c7bc). */
void ExitSubMenuToTitle(void) {
  RECT rect;

  rect.x = 0x200;
  rect.y = 0;
  rect.w = 0x100;
  rect.h = 0xE1;
  LoadImage(&rect, (char *)g_pRenderScratchPrimTop - MENU_SNAPSHOT_BYTES);
  DrawSync(0);
  g_nGamestate = GS_GAMEPLAY;
  InitLightVectorConstants();
  g_abHudCounterDisplayState[HUD_CTR_GEMS] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_DRAGONS] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_LIVES] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_EGGS] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_KEY] = HUD_CTR_HIDDEN;
  HandleMusicCommand(g_nCurrentMusicTrack, MUSIC_CMD_RESUME);
}

/* On death: spend a life and go to the respawn gamestate (4); with no lives
   left, go to game-over (5). Either way reset the gameplay draw cursor. */
void TriggerRespawnOrGameOver(void) {
  if (g_nExtraLives != 0) {
    g_nExtraLives--;
    g_nGamestate = GS_RESPAWN;
  } else {
    g_nGamestate = GS_GAME_OVER;
  }
  g_nRespawnOrGameOverMode = 0;
  g_nRespawnOrGameOverFrame = 0;
}

/* Finishes the respawn->gameplay transition: returns to GS_GAMEPLAY, copies the
   world fog RGB into both frame draw-env background-fill colors, then rebuilds
   the light vector constants. (0x8002c8a4, 112 bytes.) */
void EndRespawnToGameplay(void) {
  u_char r = ((u_char *)&g_dwWorldFogColor)[0];
  u_char g = ((u_char *)&g_dwWorldFogColor)[1];
  u_char b = ((u_char *)&g_dwWorldFogColor)[2];

  g_nGamestate = GS_GAMEPLAY;
  g_FrameDrawEnv0.r0 = r;
  g_FrameDrawEnv0.g0 = g;
  g_FrameDrawEnv0.b0 = b;
  g_FrameDrawEnv1.r0 = r;
  g_FrameDrawEnv1.g0 = g;
  g_FrameDrawEnv1.b0 = b;
  InitLightVectorConstants();
}

/* Empty stub at 0x8002C914 — sits just before Gamestate06SubstateStub_Noop and
   is itself a do-nothing handler. Bare `jr $ra; nop`. */
void StubRewardHook(void) {}

/* Empty substate handler for gamestate 6 — a do-nothing slot in the substate
   dispatch table. Compiles to the bare `jr $ra; nop` at 0x8002C91C. */
void Gamestate06SubstateStub_Noop(void) {}

/* Begins the dragon-rescue statue cutscene overlay (GS_DRAGON_RESCUE): seeds
   the cutscene substate/timer globals, bumps both dragon counters (per-level +
   game-wide total, priming the HUD cache), blocks gameplay, flags the linked
   actor's anim byte, anchors the aura star, and fires the rescue
   chime. Two statues are rewritten first: in Artisans home, rescue cutscene
   1 becomes 2 once Stone Hill, Dark Hollow, Town Square or Sunny Flight has
   been exited; in Gnorc Gnexus, once Gnasty Gnorc's level (index 33) has been
   exited, the statue gets cutscene 1 with its own camera end pose, orbit,
   dragon yaw and name (DragonStatueState). (0x8002c924, 584 bytes)

   The shared literal 1 for the two flag stores is a `$4` register local: as a
   pseudo it outranks the `dragons` pointer in local-alloc and takes $v1, which
   pushes the dragons chain and world into the wrong registers. */
void BeginDragonRescue(Actor *actor) {
  DragonStatueState *st;
  int world;
  int *dragons;
  int prevDragons;
  int idx;
  /* Hard-register pins (A374 / F14j): each holds the register the
     original uses for it, which settles the competing temps. */
  register int one asm("$4");
  st = actor->state;
  if (((g_nActiveLevelId == LEVEL_ARTISANS) && (st->cutsceneId == 1)) &&
      ((((g_abLevelExited[LEVEL_SLOT(LEVEL_STONE_HILL)] != 0) ||
         (g_abLevelExited[LEVEL_SLOT(LEVEL_DARK_HOLLOW)] != 0)) ||
        (g_abLevelExited[LEVEL_SLOT(LEVEL_TOWN_SQUARE)] != 0)) ||
       (g_abLevelExited[LEVEL_SLOT(LEVEL_SUNNY_FLIGHT)] != 0))) {
    st->cutsceneId = 2;
  }
  if (g_nActiveLevelId == LEVEL_GNORC_GNEXUS) {
    if (g_abLevelExited[LEVEL_SLOT(LEVEL_GNASTY_GNORC)] != 0) {
      st->cutsceneId = 1;
      st->orbitYaw = 0x800;
      st->orbitRadius = 0xcc8;
      st->camPos[2] = 0x2561;
      st->camEuler[0] = 0xffc;
      st->camEuler[1] = 0x6a;
      st->camEuler[2] = 0xbb;
      st->dragonYaw = 0x800;
      st->camPos[0] = 0x134bf;
      st->camPos[1] = 0x13ce9;
      st->dragonName = 0x10;
    }
  }
  g_nGamestate = GS_DRAGON_RESCUE;
  world = g_nTotalDragonsRescued;
  g_nHudDragonCachedCount = world;
  dragons = &g_anLevelDragonsRescued[g_nLevelIndex];
  prevDragons = *dragons;
  one = 1;
  g_nCinematicActive = one;
  g_nSkipGamestateDraw = one;
  *dragons = prevDragons + 1;
  g_anDragonRescueBlock[DRW_SUBSTATE] = DRAGON_RESCUE_AURA;
  g_anDragonRescueBlock[DRW_STREAM_STATE] = RESCUE_STREAM_READ;
  g_anDragonRescueBlock[DRW_TIMER] = 0;
  g_anDragonRescueBlock[DRW_SECONDARY_TIMER] = 0;
  g_anDragonRescueBlock[DRW_PAD_HOLD_TIMER] = 0;
  g_anDragonRescueBlock[DRW_AUDIO_FIRED] = 0;
  g_anDragonRescueBlock[DRW_SOURCE_ACTOR] = (int)actor;
  g_nTotalDragonsRescued = world + 1;
  g_anDragonRescueBlock[DRW_TYPE_SNAPSHOT] = st->cutsceneId;
  if (st->partner != (-1)) {
    g_pActorListBase[st->partner].anim = CHECKPT_ANIM_FLASH;
  }
  CopyVector(g_anDragonRescueAuraStarAnchor, &actor->posX);
  g_anDragonRescueAuraStarAnchor[2] += 0x40;
  PlaySoundEffect(
      LEVEL_SFX(SFX_RESCUE_START),
      (Actor *)&g_anDragonRescueBlock[DRW_SOURCE_ACTOR], SFX_MODE_PANNED,
      &((Actor *)g_anDragonRescueBlock[DRW_SOURCE_ACTOR])->voiceRow);
}

/* Leave the dragon-rescue gamestate back into the world
   (0x8002CB6C, GS_GAMEPLAY): unblock gameplay, arm the rescued dragon's
   checkpoint (its partner actor, CHECKPT_HELD while the statue stood, goes
   to CHECKPT_ARMED with anim CHECKPT_ANIM_REST), reset Spyro, re-seed the
   pad-snapshot countdown and point the camera anchor at Spyro's world
   position, retarget the dynamic camera params (yaw re-biased to Spyro's
   body yaw, wrapped to 12 bits), restore the superflame timer, reset the
   camera and HUD (all five counters hidden), and re-issue the current music
   track (command 8).

   Shares its tail with EndSaveMenuToWorld. */

void EndDragonRescueToWorld(void) {
  int **cam;
  int *dyn;
  int *snap;
  int id;
  Actor *rec;
  int timer;

  id = ((DragonStatueState *)g_pDragonRescueSourceActor->state)->partner;
  g_nGamestate = GS_GAMEPLAY;
  g_nCinematicActive = 0;
  if (id != -1) {
    rec = &g_pActorListBase[id];
    rec->anim = CHECKPT_ANIM_REST;
    rec->mode = CHECKPT_ARMED;
  }
  ResetSpyroEntity(RESET_SPYRO_FULL);
  cam = g_apCameraAnchorBlock;
  snap = g_anSpyroInputBlankBlock;
  snap[0] = 0xC;
  cam[0] = &snap[-0x7C];
  timer = g_nSpyroSuperflameTimerSave;
  dyn = g_anCameraDynamicTargetParams;
  g_nCameraCurrentMode = CAM_MODE_STAND;
  g_nCameraNextMode = CAM_MODE_STAND;
  g_pCameraTargetParams = dyn;
  g_nSpyroSuperflameTimer = timer;
  g_nCameraParamsReferenceYaw = g_nSpyroBodyYaw;
  ComputeCameraToTargetAngle((int *)((char *)cam - 0xA8)); /* g_anCameraPos */
  CopyWords(dyn, ((char *)cam) - 0x40, 0x18);              /* g_nCameraYaw */
  dyn[0] = (dyn[0] + g_nCameraParamsReferenceYaw) & ANGLE12_MASK;
  ResetCameraStateToTarget();
  InitHudCounters(0);
  g_abHudCounterDisplayState[HUD_CTR_GEMS] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_DRAGONS] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_LIVES] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_EGGS] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_KEY] = HUD_CTR_HIDDEN;
  HandleMusicCommand(g_nCurrentMusicTrack, MUSIC_CMD_RESUME);
}

/* g_abSpyroAnimPrevBlock as an INT array — +0 anim-prev byte, -6 words
   g_anSpyroWorldPos[0], -4 words g_anSpyroWorldPos[2]; the whole block must
   come off one held base. */
#define SPY_PREV ((int *)g_abSpyroAnimPrevBlock)

/* 0x8002ccc8 (868 bytes) — enter the save-menu gamestate (0xB) from the world,
   snapshotting the scene around the save host (type 110) Spyro stopped beside.
   Freezes gameplay, picks the initial cursor (0 when g_nSaveSlotLoaded is set,
   else 2), plays the level's menu sound, resets Spyro and forces anim state 1,
   then captures the yaw and distance from Spyro to the host's anchor actor, to
   the host itself, to Sparx (when present) and to the camera into the
   g_nSaveMenu*From/To keyframe block (0x80078D2C..0x80078D74: polar
   angle/distance/z about Spyro for the host, Sparx and the camera, plus Spyro's
   from/to yaw), and seeds the menu pan targets.

   The SPY_PREV alias is an incomplete INT
   array reached by element index, and the empty do/while right after the
   offset-0 `sb` that materialises its base ends cse's ebb so the -24/-16
   accesses stay off the held base (A200).  Everything the original emits above
   that store (the yaw load) is computed above it in source (A253), and the
   four byte stores below it are volatile so their order is pinned.  The second
   barrier keeps the -16 read below the batch-read g_nSaveMenuHostFromZ /
   g_nSaveMenuHostToAngle stores. `rec` is built in int arithmetic so the addu
   keeps idx*88 as its first operand (A149).  The rec+0x10 read goes through a
   `$2` register local: as a pseudo it is the subu's first dying input and
   local-alloc ties the ArcTan2 y argument to it; as a hard register the tie
   falls to posY, which then lands in $a1 as in the original. */
void BeginSaveMenuFromWorld(Actor *actor) {
  int vec[3];
  int owner;
  char *rec;
  int camYaw;
  int yaw;
  int py;
  int z;
  int yw;

  owner = ACTOR_RAW(actor, int, state); /* actor->state moves the load (A400) */
  g_nGamestate = GS_SAVE_MENU;
  g_nCinematicActive = 1;
  g_nSkipGamestateDraw = 1;
  g_nSaveMenuMode = SAVE_MODE_SWING_IN;
  g_nSaveMenuAnimPhase = 0;
  g_nSaveMenuSubstate = SAVE_SCREEN_MAIN;
  if (g_nSaveSlotLoaded != 0) {
    g_nSaveMenuCursor = SAVE_ROW_SAVE;
  } else {
    g_nSaveMenuCursor = SAVE_ROW_CONTINUE;
  }
  g_pSaveMenuHostActor = actor;
  g_nSpyroSuperflameTimerSave = g_nSpyroSuperflameTimer;
  PlaySoundEffect(LEVEL_SFX(SFX_MENU_OPEN), 0, SFX_MODE_PANNED, 0);
  ResetSpyroEntity(RESET_SPYRO_FULL);
  yaw = g_nSpyroBodyYaw;
  g_abSpyroAnimPrevBlock[0] = 1;
  EBB_BREAK();
  py = g_nSpyroWorldPosY;
  *(volatile unsigned char *)&g_bSpyroAnimCurrent = SPYRO_ANIMID_WALK;
  *(volatile unsigned char *)&g_bSpyroFramePrev = 0;
  *(volatile unsigned char *)&g_bSpyroAnimFrame = 1;
  *(volatile unsigned char *)&g_bSpyroSubFrameTimer = 0;
  g_nSaveMenuSpyroFromYaw = yaw;
  /* state word 1 is the pool index of the host's anchor actor; the
     int add before the cast is load-bearing (((int *)owner)[1] reorders cse) */
  rec = (char *)(FIELD_RAW(owner, SaveHostState, int, anchor) * sizeof(Actor) +
                 (int)g_pActorListBase);
  {
    register int ry asm("$2");
    ry = ((Actor *)rec)->posY;
    g_nSaveMenuSpyroToYaw =
        ArcTan2(((Actor *)rec)->posX - SPY_PREV[-6], ry - py, ATAN_ANGLE12);
  }
  g_nSaveMenuHostFromAngle =
      ArcTan2(ACTOR_RAW(actor, int, posX) - SPY_PREV[-6],
              actor->posY - g_nSpyroWorldPosY, ATAN_ANGLE12);
  SubtractVector(vec, &actor->posX, &SPY_PREV[-6]);
  g_nSaveMenuHostFromDist = VectorLength(vec, VEC_LEN_XY);
  z = ACTOR_RAW(actor, int, posZ);
  yw = g_nSaveMenuSpyroToYaw;
  g_nSaveMenuHostToDist = 0x480;
  g_nSaveMenuHostFromZ = z;
  g_nSaveMenuHostToAngle = yw;
  EBB_BREAK();
  g_nSaveMenuHostToZ = SPY_PREV[-4] + 0x170;
  if (g_pSparxActor != 0) {
    g_nSaveMenuSparxFromAngle =
        ArcTan2(g_pSparxActor->posX - SPY_PREV[-6],
                g_pSparxActor->posY - g_nSpyroWorldPosY, ATAN_ANGLE12);
    SubtractVector(vec, &g_pSparxActor->posX, &SPY_PREV[-6]);
    g_nSaveMenuSparxFromDist = VectorLength(vec, VEC_LEN_XY);
    g_nSaveMenuSparxToDist = 0x600;
    g_nSaveMenuSparxToAngle =
        (g_nSaveMenuSpyroToYaw + ANGLE12_HALF) & ANGLE12_MASK;
  }
  g_nSaveMenuCameraFromAngle =
      ArcTan2(g_anCameraPos[0] - SPY_PREV[-6],
              g_anCameraPos[1] - g_nSpyroWorldPosY, ATAN_ANGLE12);
  SubtractVector(vec, g_anCameraPos, &SPY_PREV[-6]);
  g_nSaveMenuCameraFromDist = VectorLength(vec, VEC_LEN_XY);
  g_nSaveMenuXOffset = 0xB0;
  g_nSaveMenuCameraFromZ = g_anCameraPos[2];
  camYaw = (g_nSaveMenuSpyroToYaw + 0x30B) & ANGLE12_MASK;
  g_nSaveMenuCameraToAngle = camYaw;
  if (AbsAngleDelta12(g_nSaveMenuCameraFromAngle, camYaw) > 0x400) {
    g_nSaveMenuXOffset = 0;
    g_nSaveMenuCameraToAngle = (g_nSaveMenuSpyroToYaw - 0x30B) & ANGLE12_MASK;
  }
  g_nSaveMenuCameraToDist = 0x4C9;
  g_nSaveMenuCameraToZ = SPY_PREV[-4] + 0x56B;
}

#undef SPY_PREV

/* 0x8002d02c, 324 bytes — leave the save/load menu back into the world
   (GS_GAMEPLAY): unblock gameplay, rebuild the light constants, zero Spyro's
   swing-euler target and reset him, re-seed the swing-euler defaults and the
   pad-snapshot countdown, point the camera anchor 0x1A4 below the swing-target
   block, retarget the dynamic camera params (yaw re-biased to Spyro's body yaw
   and wrapped to 12 bits), restore the superflame timer, reset the camera and
   the HUD (all five counters hidden), and re-issue the current music track. */

void EndSaveMenuToWorld(void) {
  int *dyn;

  g_nGamestate = GS_GAMEPLAY;
  g_nCinematicActive = 0;
  InitLightVectorConstants();
  ZeroVector(g_anSpyroSwingTargetBlock);
  ResetSpyroEntity(RESET_SPYRO_FULL);

  g_nSpyroSwingEulerPrev1 = 0x100;
  g_bSpyroSwingByteEuler1 = 0x10;
  g_nSpyroInputBlankCountdown = 8;
  g_apCameraAnchorBlock[0] =
      &g_anSpyroSwingTargetBlock[SPY_POS - SPY_SWING_TARGET];

  dyn = g_anCameraDynamicTargetParams;
  g_nCameraCurrentMode = CAM_MODE_STAND;
  g_nCameraNextMode = CAM_MODE_STAND;
  g_pCameraTargetParams = dyn;
  g_nSpyroSuperflameTimer = g_nSpyroSuperflameTimerSave;
  g_nCameraParamsReferenceYaw =
      g_anSpyroMoveTargetYawBlock[SPY_BODY_YAW - SPY_MOVE_TARGET_YAW];

  ComputeCameraToTargetAngle((int *)CAM_POS_VIA_ANCHOR);
  CopyWords(dyn, CAM_YAW_VIA_ANCHOR, 0x18);
  dyn[0] = (dyn[0] + g_nCameraParamsReferenceYaw) & ANGLE12_MASK;

  ResetCameraStateToTarget();
  InitHudCounters(0);

  g_abHudCounterDisplayState[HUD_CTR_GEMS] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_DRAGONS] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_LIVES] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_EGGS] = HUD_CTR_HIDDEN;
  g_abHudCounterDisplayState[HUD_CTR_KEY] = HUD_CTR_HIDDEN;

  HandleMusicCommand(g_nCurrentMusicTrack, MUSIC_CMD_RESUME);
}

/* The yaw read goes through the g_anSpyroMoveTargetYawBlock alias (+0x50 ==
 * g_nSpyroBodyYaw, same reloc, identical bytes) and NOT through the scalar
 * symbol, because it has to be an ARRAY_REF. That is what orders it after the
 * camera-anchor store. gcc 2.7.2's sched.c calls true_dependence, whose
 * heuristic drops the dependence when one MEM is in a struct with a varying
 * address and the other is neither: the anchor store is `(mem/s (reg s1))`
 * (MEM_IN_STRUCT_P, register address) and a scalar-symbol read is neither, so
 * the pair is declared independent and the load floats above the store. An
 * ARRAY_REF sets MEM_IN_STRUCT_P on the load, the exclusion no longer applies,
 * and the load is pinned below the store exactly as the original has it.
 * Volatile on both sides (A208) also pins it, but it then pins the flame-timer
 * load too and costs the ComputeCameraToTargetAngle delay slot (+1 insn). */

/* Enter the story-intro gamestate (0x8002d170, GS_TITLE): zero the
   0x5C-byte mode block, prime the SPU common attr (mask 0xC0, master volume
   0x5000 into +0x10/+0x12) alongside the 0x3FFF/0x5000 volume shadow globals,
   drop SFX/music volume to 10, park the cutscene scratch pointer 0x2000
   below the render scratch region, and reseed the RNG. The 0xC0 store and the
   SetSpuCommonAttr argument address the attr block 0x320 below
   g_nSfxMixVolume_blk, which gcc holds as the one materialized base across the
   call. */

void BeginStoryIntro(void) {
  int *blk;
  int back;
  int vol;
  g_nGamestate = GS_TITLE;
  FillWord(&g_nTitleMode, 0, 0x5C);
  blk = g_nSfxMixVolume_blk;
  blk[0] = SPU_VOLUME_MAX;
  vol = 0x5000;
  g_nCdAudioVolume = vol;
  g_nSfxVolume = 0xA;
  *((short *)(&g_abSpuCommonAttr[0x10])) = vol;
  *((short *)(&g_abSpuCommonAttr[0x12])) = vol;
  back = 200;
  g_nMusicVolume = 0xA;
  blk[-back] = (short)0xC0;
  g_pStoryIntroStreamCursor = ((char *)g_pRenderScratchRegionBase) - 0x2000;
  SetSpuCommonAttr((unsigned char *)(blk - 200));
  g_nPortalDistanceFogEnabled = 0;
  g_pPathTableHead->frame = 0;
  srand(0x159);
}

/* Enter the credits gamestate (0x8002d228, GS_CREDITS): clear the whole
   VRAM framebuffer, silence all sound, optionally reload the actor mesh tables
   (param nonzero), reset Spyro, zero the credits/returning-home phase globals
   (g_nCreditsPageIndex only when it is not the level-select magic 0xA), then
   kick off the synchronous 0x258-sector credits asset read from the CD base
   LBA. */

void BeginCreditsSequence(int reloadMeshes) {
  int unused[8];
  RECT rect;
  rect.w = SCREEN_W;
  rect.x = 0;
  rect.y = 0;
  rect.h = SCREEN_H * 2;
  ClearImage(&rect, 0, 0, 0);
  DrawSync(0);
  StopAllSoundExceptMask(0);
  TickSpuPerFrame();
  if (reloadMeshes != 0) {
    LoadActorMeshTables();
  }
  ResetSpyroEntity(RESET_SPYRO_FULL);
  g_nGamestate = GS_CREDITS;
  g_nCreditsMode = CREDITS_MODE_BUILD;
  g_nCreditsFirstVisibleLine = 0;
  g_nCreditsLastVisibleLine = 0;
  g_nCreditsLineCount = 0;
  g_pCreditsScrollHeader = 0;
  g_pCreditsScratchBuf = 0;
  g_nReturningHomePhase = 0;
  if (g_nCreditsPageIndex != 0xA) {
    g_nCreditsPageIndex = 0;
  }
  {
    int lba = *(volatile int *)&g_nCdBaseLba;
    int size = *(volatile int *)&g_nCreditsSceneByteSize;
    int offset = *(volatile int *)&g_nCreditsSceneSectorOffset;
    int *dst = *(int *volatile *)&g_pSharedLoadBuffer;
    CdReadSyncSectors(lba, dst, size, offset, CD_STALL_TIMEOUT_TICKS);
  }
}

/* Enter the scripted-cutscene gamestate (0x8002d338, GS_CUTSCENE). The
   cutscene id is g_nCutsceneId: 0 title backdrop, 1 opening story, 2/3
   ending. Steps: reset the
   generic countdown fade and the path-table head, raise the storm flag for
   the opening story (id 1), then build the cutscene sample-bank entry at
   g_CutsceneSoundDef (sample 0x1010, per-id pitch from
   g_nCutsceneSamplePitch) and trigger it via PlaySoundEffect with the
   dragon-rescue voice marker. */
void BeginCutscene(void) {
  register int id;
  CutscenePath *head;
  int *entry;

  g_nGamestate = GS_CUTSCENE;
  g_nScreenFadeLevel = 0;
  head = g_pPathTableHead;
  id = g_nCutsceneId;
  head->frame = 0;
  if (id == CUTSCENE_OPENING) {
    g_nPortalDistanceFogEnabled = 1;
  }

  entry = g_CutsceneSoundDef;
  g_pActiveCutsceneSoundDef = (char *)entry;
  /* g_pActiveCutsceneSoundDef is the volatile view of
     g_pLevelSampleBankEntries: the original reloads the pointer before every
     field store. */
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, int, nSampleAddr) = SPU_SAMPLE_BASE;
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, int, nLoopAddr) = -1;
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, short, nVolume) = 0x50;
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, short, nPitch) =
      (&g_nCutsceneSamplePitch)[id];
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, short, nPitchSpread) = 0;
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, short, nPitchStep) = 0;
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, int, nPitchMode) = SAMPLE_PITCH_AROUND;
  g_nSfxOverrideVolL = SPU_VOLUME_MAX;
  g_nSfxOverrideVolR = SPU_VOLUME_MAX;
  g_nSfxOverrideFlags = SFX_OVERRIDE_VOLUME;

  PlaySoundEffect(0, 0, SFX_MODE_PANNED, &g_bSceneSfxVoiceMarker);
}

/* Special-cutscene dispatcher (0x8002d440), by g_nCutsceneId, at the end of a
   cutscene. The opening: full story-intro restart — clear VRAM, silence sound,
   reload actor meshes, GS_TITLE with the 0x5C-byte mode block zeroed
   (TITLE_MODE_STORY_INTRO, exit path TITLE_EXIT_LOADED_GAME), fresh game state,
   level 0xA. The Gnasty ending: credits without a mesh reload. The Loot ending:
   credits with a mesh reload, seeding g_nCreditsPageIndex = 0xA so
   BeginCreditsSequence keeps it. */

void DispatchSpecialCutscene(void) {
  RECT rect;
  int id = g_nCutsceneId;
  if (id == CUTSCENE_OPENING) {
    int *mode = g_anTitleModeBlock;
    rect.w = SCREEN_W;
    rect.x = 0;
    rect.y = 0;
    rect.h = SCREEN_H * 2;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    StopAllSoundExceptMask(0);
    TickSpuPerFrame();
    LoadActorMeshTables();
    g_nGamestate = GS_TITLE;
    FillWord(mode, 0, 0x5C);
    mode[0] = TITLE_MODE_STORY_INTRO;
    g_nStoryIntroSubstate = STORY_INTRO_ARM;
    g_nTitleExitPath = TITLE_EXIT_LOADED_GAME;
    InitNewGameState();
    g_nActiveLevelId = LEVEL_ARTISANS;
    g_nSkipGamestateDraw = 1;
  } else if (id == CUTSCENE_GNASTY_ENDING) {
    BeginCreditsSequence(0);
    g_nSkipGamestateDraw = 1;
  } else if (id == CUTSCENE_LOOT_ENDING) {
    g_nCreditsPageIndex = 0xA;
    BeginCreditsSequence(1);
    g_nSkipGamestateDraw = 1;
  }
}
