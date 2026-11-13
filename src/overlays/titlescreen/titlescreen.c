#include "actor.h"
#include "cd_stream.h"
#include "frame_arena.h"
#include "sparx.h"
#include "spu.h"
#include "title_state.h"
#include "titlescreen.h"

/* Titlescreen_PlayUiSound (0x8007AAD4, titlescreen overlay).
 *
 * Queue one of the title-screen UI sounds: point the SPU driver at the shared
 * cutscene sound definition, fill it for the requested sample, and fire it at
 * full volume through the dragon-cutscene voice.
 *
 * Externs below keep our splat autolabel names for the main-EXE symbols,
 * which the overlay link PROVIDE()s by address.
 * The SPU-driver fields must be accessed through one struct (not separate scalar
 * externs): gcc 2.7.2 reloads the sound-definition pointer after every store
 * through a struct field, which is what the original codegen shows — a plain
 * global pointer gets constant-folded into direct stores and diverges.
 *
 * Verified byte-identical inside the relinked titlescreen.ovl.
 */

typedef struct {
  int nAddr;              /* 0x0 SPU sample address */
  int nLoopAddr;          /* 0x4 */
  short nVolume;          /* 0x8 */
  short nPitch;           /* 0xA */
  short nPitchVariance;   /* 0xC */
  short nPitchMultiplier; /* 0xE */
  int nVarianceType;      /* 0x10 */
} SoundDef;

typedef struct {
  SoundDef *pSoundDef;          /* 0x0  (0x800761D0) active sound definition */
  int pad04;                     /* 0x4 */
  int pad08;                     /* 0x8 */
  int nNextSoundOverrideFlags;  /* 0xC  (0x800761DC) g_nSfxOverrideFlags */
  int pad10;                    /* 0x10 */
  int pad14;                    /* 0x14 */
  short nVolumeOverrideLeft;    /* 0x18 (0x800761E8) g_nSfxOverrideVolL */
  short nVolumeOverrideRight;   /* 0x1A (0x800761EA) g_nSfxOverrideVolR */
} SpuDriver;

extern SpuDriver g_pLevelSampleBankEntries;  /* the SPU driver state */
extern SoundDef g_CutsceneSoundDef;   /* the shared cutscene sound definition */

void Titlescreen_PlayUiSound(int nSoundId) {
  g_pLevelSampleBankEntries.pSoundDef = &g_CutsceneSoundDef;
  g_pLevelSampleBankEntries.pSoundDef->nAddr = g_anTitleSampleOffsets[nSoundId] + SPU_SAMPLE_BASE;
  g_pLevelSampleBankEntries.pSoundDef->nLoopAddr = -1;
  g_pLevelSampleBankEntries.pSoundDef->nVolume = 0x50;
  g_pLevelSampleBankEntries.pSoundDef->nPitch = 0x800;
  g_pLevelSampleBankEntries.pSoundDef->nPitchVariance = 0;
  g_pLevelSampleBankEntries.pSoundDef->nPitchMultiplier = 0;
  g_pLevelSampleBankEntries.pSoundDef->nVarianceType = 0;
  g_pLevelSampleBankEntries.nNextSoundOverrideFlags = SFX_OVERRIDE_VOLUME;
  g_pLevelSampleBankEntries.nVolumeOverrideLeft = SPU_VOLUME_MAX;
  g_pLevelSampleBankEntries.nVolumeOverrideRight = SPU_VOLUME_MAX;
  PlaySoundEffect(0, 0, SFX_MODE_PANNED, (unsigned char *)&g_bSceneSfxVoiceMarker);
}

/* Titlescreen_Update (0x8007ABAC, titlescreen overlay).
 *
 * The whole title-screen state machine: the intro cutscene timing and skip,
 * CD streaming of the title sprite sheet, the memory-card menu tree (detect /
 * select / read / format / create-save, with retry and checksum-repair paths),
 * the save-file picker, and the demo-timeout fades. Runs once per frame before
 * Titlescreen_Draw.
 *
 * Owns the overlay's rodata: the BASCUS save-file name string (0x4) and its
 * substate jump table (0x1C, jtbl_8007AA54) — both emitted by this object's
 * .rodata and slot-placed.
 *
 * The sprite-sheet CD read pins its CD handle to $a0 and the g_nTitleSceneSectorOffset
 * argument to $a3. As pseudos, the handle's volatile load and the a3 load are
 * both birthing-boosted, and they crowd the `- 0x40000` constant and the
 * buffer pointer into one sched1 priority group, which hands $a1 to the
 * pointer. As hard-register sets they are LUID-ordered stragglers instead:
 * the handle loads first, the a3 load last, and the constant keeps $a1.
 */


static const char s_SaveFileName[20] = "BASCUS-94228SPYRO";

#define SWAP_MENU_SELECTION                                                    \
  g_nTitleMode.animTimer = 16;                                                   \
  g_nTitleMode.selection = 1 - g_nTitleMode.selection;               \
  Titlescreen_PlayUiSound(TITLE_SFX_MOVE);

#define RELOAD_MEMCARDS                                                        \
  g_nTitleMode.subState = TITLE_CARD_PROBE;                            \
  g_nTitleMode.menuState = 0;                                                      \
  MemCardSync(0, (long *)&g_nTitleMode.mcCmd,                    \
                (long *)&g_nTitleMode.mcResult);

#define START_GAME(EXIT_PATH)                                                  \
  g_nTitleMode.menuState = TITLE_MENU_FADE_OUT;                                                      \
  g_nTitleMode.exitPath = EXIT_PATH;                                           \
  g_nSaveSlotLoaded_s.fileTag = g_nTitleMode.file->header.clut[15];         \
  g_nSaveSlotLoaded_s.loaded = 1;                                              \
  g_nSaveSlotLoaded_s.cardPort = g_nTitleMode.cardPort << 4;                \
  g_nSaveSlotLoaded_s.slot = g_nTitleMode.selection;                           \
  MemCardSync(0, (long *)&g_nTitleMode.mcCmd,                    \
                (long *)&g_nTitleMode.mcResult);

void Titlescreen_Update(void) {
  int old_state =
      g_nTitleMode.mode < TITLE_MODE_SAVE_SELECT ? g_nTitleMode.subState : g_nTitleMode.menuState;

  g_pPathTableHead->frame += 2;

  if (g_pPathTableHead->frame < 1100) {
    g_nTitleMode.idleFrames = 0;
    if (g_nTitleMode.subState == TITLE_ATTRACT_FLY_IN && g_pPathTableHead->frame >= 300 &&
        (g_dwPadPressed.held & (PAD_START | PAD_CROSS)) != 0) {
      StopSoundVoiceByIndex(g_bSceneSfxVoiceMarker, VOICE_RELEASE);
      g_pPathTableHead->frame = 1170;
    }
  } else {
    if (g_pPathTableHead->frame == 1344 && (GetRandomU32() & 3) != 0) {
      g_pPathTableHead->frame = 1172;
    } else if (g_pPathTableHead->frame >= 1530) {
      g_pPathTableHead->frame = 1172;
    }
  }

  if (g_nScreenFadeLevel)
    --g_nScreenFadeLevel;

  AdvanceCutsceneFrame(g_pPathTableHead->frame);

  g_nCameraEulerRoll.x = 0;
  g_nCameraEulerRoll.y += 16;

  switch (g_nTitleMode.mode) {
  case TITLE_MODE_ATTRACT: {
    if (g_nTitleMode.subState < TITLE_ATTRACT_FLY_IN) {
      /* kick out while we're still loading from the CD */
      RestartStalledCdRead();
      if (g_nCdBaseLba.readInFlight != 0 || CallCdSync(1, 0) != 2 ||
          (g_nCurrentMusicTrack.streamStatus & MUSIC_ST_IDLE) == 0)
        break;

      if (g_nTitleMode.subState == TITLE_ATTRACT_LOAD) {
        /* kick off loading titlescreen data from the CD */
        /* StartCdReadAsync's lba in a0 and the sector offset in a3, as in
         * the original (A374). */
        register int cd asm("$4");
        register int t asm("$7");
        cd = *((volatile int *) &g_nCdBaseLba.baseLba);
        t = g_nTitleSceneSectorOffset;
        StartCdReadAsync(cd, g_pRenderScratchRegionBase - TIM_PAGE_BYTES, TIM_PAGE_BYTES,
                      t, CD_STALL_TIMEOUT_TICKS);
        g_nTitleMode.subState = TITLE_ATTRACT_LOAD_WAIT;
      } else if (g_nTitleMode.subState == TITLE_ATTRACT_LOAD_WAIT) {
        /* once loaded, initialize music and upload the sprite sheet to vram */
        RECT rc;
        g_nCurrentMusicTrack.track = 20;
        g_nCurrentMusicTrack.streamTimeout = 155520000;
        HandleMusicCommand(20, MUSIC_CMD_PLAY);
        setRECT(&rc, 512, 256, 512, 256);
        LoadImage(&rc, g_pRenderScratchRegionBase - TIM_PAGE_BYTES);
        g_nTitleMode.subState = TITLE_ATTRACT_FLY_IN;
      }
    } else if (g_nTitleMode.subState == TITLE_ATTRACT_FLY_IN) {
      /* titlescreen sequence is playing, just waiting for a bit */
      if (g_pPathTableHead->frame < 1169) {
        g_nTitleMode.animTimer = 0;
      } else {
        if (g_nTitleMode.animTimer == 1) {
          Titlescreen_PlayUiSound(TITLE_SFX_LOGO_IN);
        }
        if (g_nTitleMode.animTimer >= 16)
          g_nTitleMode.subState = TITLE_ATTRACT_PRESS_START;
      }
    } else if (g_nTitleMode.subState == TITLE_ATTRACT_PRESS_START) {
      /* ready for button inputs or to time out to show the demo */
      if ((g_dwPadPressed.held & (PAD_L1 | PAD_TRIANGLE)) ==
          (PAD_L1 | PAD_TRIANGLE)) {
        StopAllSoundExceptMask(0); /* stop all sounds */
        Titlescreen_PlayUiSound(TITLE_SFX_SUCCESS);
        TickSpuPerFrame();
        while (g_nCdBaseLba.readInFlight != 0 || CallCdSync(1, 0) != 2 ||
               (g_nCurrentMusicTrack.streamStatus & MUSIC_ST_IDLE) == 0) {
          RestartStalledCdRead();
          TickActiveSoundVoices(); /* update sounds */
          TickCdMusicStream();
        }
        LaunchCrashDemo(); /* crash-demo launch */
      } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS)) {
        g_nTitleMode.subState = TITLE_ATTRACT_LOGO_OUT;
        Titlescreen_PlayUiSound(TITLE_SFX_START);
      } else if (g_nTitleMode.idleFrames >= 990) {
        g_nTitleMode.subState = TITLE_ATTRACT_IDLE_FADE;
      }
    } else if (g_nTitleMode.subState == TITLE_ATTRACT_LOGO_OUT) {
      /* animate out the logo, move to mem card screen */
      if (g_nTitleMode.animTimer == 16) {
        Titlescreen_PlayUiSound(TITLE_SFX_LOGO_OUT);
      }
      if (g_nTitleMode.animTimer >= 32) {
        g_nTitleMode.mode = TITLE_MODE_CARD_CHECK;
        g_nTitleMode.subState = TITLE_CARD_PROBE;
        g_nTitleMode.menuState = 0;
        MemCardOpenSession(); /* MemCardStart */
      }
    } else if (g_nTitleMode.subState == TITLE_ATTRACT_IDLE_FADE) {
      /* timed out: fade out for 16 ticks and then move to demo mode */
      if (g_nTitleMode.animTimer == 0) {
        Titlescreen_PlayUiSound(TITLE_SFX_FADE_OUT);
      }
      g_nScreenFadeLevel = g_nTitleMode.animTimer;
      if (g_nScreenFadeLevel > 15)
        g_nScreenFadeLevel = 15;
      if (g_nTitleMode.animTimer >= 16) {
        RECT rc;
        g_nTitleMode.mode = TITLE_MODE_STORY_INTRO;
        g_nTitleMode.menuState = 0;
        g_nTitleMode.exitPath = TITLE_EXIT_DEMO;
        setRECT(&rc, 0, 0, 512, 480);
        ClearImage(&rc, 0, 0, 0);
        DrawSync(0); /* DrawSync */
      }
    }
    break;
  }
  case TITLE_MODE_CARD_CHECK: {
    switch (g_nTitleMode.subState) {
    case TITLE_CARD_PROBE: {
      /* wait for async mem card function to finish */
      if (MemCardSync(1, (long *)&g_nTitleMode.mcCmd,
                        (long *)&g_nTitleMode.mcResult) == 0)
        break;

      if (g_nTitleMode.menuState == 0) {
        /* try to ping port A for a mem card */
        Titlescreen_PlayUiSound(TITLE_SFX_CARD_ACCESS);
        MemCardLoad(0 /* port A card 0 */);
        g_nTitleMode.menuState = 1;
      } else if (g_nTitleMode.menuState == 1) {
        /* pull memcard state for port A */
        g_nTitleMode.cardStatus[0] = g_nTitleMode.mcResult;

        if (g_nTitleMode.cardStatus[0] == McErrNewCard)
          g_nTitleMode.cardStatus[0] = McErrNone;
        if (g_nTitleMode.cardStatus[0] == McErrNotFormat) {
          g_nTitleMode.cardStatus[0] = McErrNone;
          g_nTitleMode.needsFormat[0] = 1;
        } else
          g_nTitleMode.needsFormat[0] = 0;

        /* now go and ping port B */
        MemCardLoad(0x10 /* port B card 0 */);
        g_nTitleMode.menuState = 2;
      } else {
        /* pull memcard state for port B */
        g_nTitleMode.cardStatus[1] = g_nTitleMode.mcResult;

        if (g_nTitleMode.cardStatus[1] == McErrNewCard)
          g_nTitleMode.cardStatus[1] = McErrNone;
        if (g_nTitleMode.cardStatus[1] == McErrNotFormat) {
          g_nTitleMode.cardStatus[1] = McErrNone;
          g_nTitleMode.needsFormat[1] = 1;
        } else {
          g_nTitleMode.needsFormat[1] = 0;
        }

        /* figure out what screen to go to for our A/B memcard states */
        if (g_nTitleMode.cardStatus[0] == McErrNone &&
            g_nTitleMode.cardStatus[1] == McErrNone) {
          /* both ports have a valid, formatted memcard, go to select menu */
          g_nTitleMode.subState = TITLE_CARD_PICK_SLOT;
          g_nTitleMode.selection = 0;
        } else if (g_nTitleMode.cardStatus[0] == McErrNone &&
                   !g_nTitleMode.needsFormat[0]) {
          /* port A is valid and doesn't need formatting: select it */
          g_nTitleMode.cardPort = 0;
          g_nTitleMode.subState = TITLE_CARD_READ;
          g_nTitleMode.menuState = 0;
          g_nTitleMode.retryCount = 0;
          g_nTitleMode.selection = 0;
        } else if (g_nTitleMode.cardStatus[1] == McErrNone &&
                   !g_nTitleMode.needsFormat[1]) {
          /* port B is valid and doesn't need formatting: select it */
          g_nTitleMode.cardPort = 1;
          g_nTitleMode.subState = TITLE_CARD_READ;
          g_nTitleMode.menuState = 0;
          g_nTitleMode.retryCount = 0;
          g_nTitleMode.selection = 0;
        } else if (g_nTitleMode.cardStatus[0] == McErrNone &&
                   g_nTitleMode.needsFormat[0]) {
          /* port A is valid but needs a format, ask if that's ok */
          g_nTitleMode.cardPort = 0;
          g_nTitleMode.subState = TITLE_CARD_FORMAT_ASK;
          g_nTitleMode.selection = 0;
        } else if (g_nTitleMode.cardStatus[1] == McErrNone &&
                   g_nTitleMode.needsFormat[1]) {
          /* port B is valid but needs a format, ask if that's ok */
          g_nTitleMode.cardPort = 1;
          g_nTitleMode.subState = TITLE_CARD_FORMAT_ASK;
          g_nTitleMode.selection = 0;
        } else {
          /* no valid cards found */
          g_nTitleMode.cardPort = -1;
          g_nTitleMode.subState = TITLE_CARD_NONE;
          g_nTitleMode.selection = 0;
        }
      }
      break;
    }
    case TITLE_CARD_READ: {
      /* wait for async mem card function to finish */
      if (MemCardSync(1, (long *)&g_nTitleMode.mcCmd,
                        (long *)&g_nTitleMode.mcResult) == 0)
        break;

      if (g_nTitleMode.menuState == 0) {
        /* read save file from the selected card */
        MemCardRead(g_nTitleMode.cardPort << 4, (char *)s_SaveFileName,
                      (unsigned long *)g_nTitleMode.file, 0,
                      sizeof(MemCardSaveFile));
        g_nTitleMode.menuState = 1;
      } else if (g_nTitleMode.menuState == 1) {
        /* save file has been read */
        if (g_nTitleMode.mcResult == McErrNone) {
          /* no errors reading, check to see if data is intact */
          if (ComputeSaveGameChecksum(
                  (u_char *)&g_nTitleMode.file->saves[0]) ==
                  g_nTitleMode.file->saves[0].checksum &&
              ComputeSaveGameChecksum(
                  (u_char *)&g_nTitleMode.file->saves[1]) ==
                  g_nTitleMode.file->saves[1].checksum &&
              ComputeSaveGameChecksum(
                  (u_char *)&g_nTitleMode.file->saves[2]) ==
                  g_nTitleMode.file->saves[2].checksum) {
            /* save checksums matched, go to save file picker */
            g_nTitleMode.mode = TITLE_MODE_SAVE_SELECT;
            g_nTitleMode.menuState = TITLE_MENU_NEW_OR_LOAD;
            g_nTitleMode.selection = 1;
            g_nSkipGamestateDraw = 1;
            Titlescreen_PlayUiSound(TITLE_SFX_SUCCESS);
          } else if (g_nTitleMode.retryCount < 2) {
            /* save checksums didn't match, retry the read up to 2 times */
            g_nTitleMode.retryCount++;
            g_nTitleMode.menuState = 0;
          } else {
            /* retries didn't work, recreate the ones that looked like garbage */
            InitNewGameState(); /* reset game */
            if (ComputeSaveGameChecksum(
                    (u_char *)&g_nTitleMode.file->saves[0]) !=
                g_nTitleMode.file->saves[0].checksum)
              BuildSaveGameBuffer(&g_nTitleMode.file->saves[0]);
            if (ComputeSaveGameChecksum(
                    (u_char *)&g_nTitleMode.file->saves[1]) !=
                g_nTitleMode.file->saves[1].checksum)
              BuildSaveGameBuffer(&g_nTitleMode.file->saves[1]);
            if (ComputeSaveGameChecksum(
                    (u_char *)&g_nTitleMode.file->saves[2]) !=
                g_nTitleMode.file->saves[2].checksum)
              BuildSaveGameBuffer(&g_nTitleMode.file->saves[2]);

            /* go to the save file picker */
            g_nTitleMode.mode = TITLE_MODE_SAVE_SELECT;
            g_nTitleMode.menuState = TITLE_MENU_NEW_OR_LOAD;
            g_nTitleMode.selection = 1;
            g_nSkipGamestateDraw = 1;
            Titlescreen_PlayUiSound(TITLE_SFX_SUCCESS);
          }
        } else if (g_nTitleMode.mcResult == McErrFileNotExist) {
          /* no save file, ask if we can create one */
          g_nTitleMode.subState = TITLE_CARD_CREATE_ASK;
          g_nTitleMode.selection = 0;
        } else {
          /* no memory card available anymore */
          g_nTitleMode.subState = TITLE_CARD_UNUSABLE;
          g_nTitleMode.selection = 0;
        }
      }
      break;
    }
    case TITLE_CARD_NONE: {
      PollMemCardSyncRing();

      if (g_nTitleMode.cardStatus[0] == McErrNone ||
          g_nTitleMode.cardStatus[0] == McErrNewCard ||
          g_nTitleMode.cardStatus[1] == McErrNone ||
          g_nTitleMode.cardStatus[1] == McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nTitleMode.animTimer == 0) {
        Titlescreen_PlayUiSound(TITLE_SFX_ERROR);
      }

      if (g_nTitleMode.animTimer >= 8) {
        if (RestartTitleCardCheckOnTriangle()) {
          /* triangle was pushed, RestartTitleCardCheckOnTriangle already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS)) {
          if (g_nTitleMode.selection == 0) {
            g_nTitleMode.subState = TITLE_CARD_NO_SAVE_WARNING;
            Titlescreen_PlayUiSound(TITLE_SFX_PROMPT);
          } else {
            RELOAD_MEMCARDS
          }
        }
      }
      break;
    }
    case TITLE_CARD_UNUSABLE: {
      PollMemCardSyncRing();

      if (g_nTitleMode.cardPort >= 0) {
        if (g_nTitleMode.cardStatus[g_nTitleMode.cardPort] != McErrNone ||
            g_nTitleMode.cardStatus[1 - g_nTitleMode.cardPort] ==
                McErrNewCard) {
          RELOAD_MEMCARDS
          break;
        }
      } else {
        if (g_nTitleMode.cardStatus[0] == McErrNone ||
            g_nTitleMode.cardStatus[0] == McErrNewCard ||
            g_nTitleMode.cardStatus[1] == McErrNone ||
            g_nTitleMode.cardStatus[1] == McErrNewCard) {
          RELOAD_MEMCARDS
          break;
        }
      }

      if (g_nTitleMode.animTimer >= 8) {
        if (RestartTitleCardCheckOnTriangle()) {
          /* triangle was pushed, RestartTitleCardCheckOnTriangle already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS)) {
          if (g_nTitleMode.selection == 0) {
            g_nTitleMode.subState = TITLE_CARD_NO_SAVE_WARNING;
            Titlescreen_PlayUiSound(TITLE_SFX_PROMPT);
          } else {
            RELOAD_MEMCARDS
          }
        }
      }
      break;
    }
    case TITLE_CARD_NO_SAVE_WARNING: {
      PollMemCardSyncRing();

      if (g_nTitleMode.cardPort >= 0) {
        if (g_nTitleMode.cardStatus[g_nTitleMode.cardPort] != McErrNone ||
            g_nTitleMode.cardStatus[1 - g_nTitleMode.cardPort] ==
                McErrNewCard) {
          RELOAD_MEMCARDS
          break;
        }
      } else {
        if (g_nTitleMode.cardStatus[0] == McErrNone ||
            g_nTitleMode.cardStatus[0] == McErrNewCard ||
            g_nTitleMode.cardStatus[1] == McErrNone ||
            g_nTitleMode.cardStatus[1] == McErrNewCard) {
          RELOAD_MEMCARDS
          break;
        }
      }

      /* mem card states still the same, process buttons after a few ticks */
      if (g_nTitleMode.animTimer >= 8) {
        if (RestartTitleCardCheckOnTriangle()) {
          /* triangle was pushed, RestartTitleCardCheckOnTriangle already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          if (g_nTitleMode.selection == 0) {
            InitNewGameState();
            g_nTitleMode.mode = TITLE_MODE_SAVE_SELECT;
            g_nTitleMode.menuState = TITLE_MENU_FADE_OUT;
            g_nTitleMode.exitPath = TITLE_EXIT_NEW_GAME;
            g_nSaveSlotLoaded_s.loaded = 0;
          } else {
            RELOAD_MEMCARDS
          }
        }
      }
      break;
    }
    case TITLE_CARD_FORMAT_ASK:
    case TITLE_CARD_FORMAT_CONFIRM: {
      PollMemCardSyncRing();

      if (g_nTitleMode.cardStatus[g_nTitleMode.cardPort] != McErrNone ||
          g_nTitleMode.cardStatus[1 - g_nTitleMode.cardPort] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nTitleMode.animTimer == 0) {
        Titlescreen_PlayUiSound(TITLE_SFX_PROMPT);
      }

      if (g_nTitleMode.animTimer >= 8) {
        if (RestartTitleCardCheckOnTriangle()) {
          /* triangle was pushed, RestartTitleCardCheckOnTriangle already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          if (g_nTitleMode.selection == 0) {
            if (g_nTitleMode.subState == TITLE_CARD_FORMAT_ASK) {
              g_nTitleMode.subState = TITLE_CARD_FORMAT_CONFIRM;
              g_nTitleMode.selection = 1;
            } else {
              g_nTitleMode.subState = TITLE_CARD_FORMATTING;
              MemCardSync(0, (long *)&g_nTitleMode.mcCmd,
                            (long *)&g_nTitleMode.mcResult);
              Titlescreen_PlayUiSound(TITLE_SFX_CARD_ACCESS);
            }
          } else {
            g_nTitleMode.subState = TITLE_CARD_UNUSABLE;
            g_nTitleMode.selection = 0;
            Titlescreen_PlayUiSound(TITLE_SFX_PROMPT);
          }
        }
      }
      break;
    }
    case TITLE_CARD_FORMATTING: {
      if (g_nTitleMode.animTimer >= 2) {
        g_nTitleMode.mcResult =
            MemCardFormat(g_nTitleMode.cardPort << 4);
        if (g_nTitleMode.mcResult == McErrNone) {
          g_nTitleMode.cardStatus[g_nTitleMode.cardPort] = McErrNone;
          g_nTitleMode.subState = TITLE_CARD_FORMAT_DONE;
          g_nTitleMode.selection = 0; /* continue */
          Titlescreen_PlayUiSound(TITLE_SFX_SUCCESS);
        } else {
          g_nTitleMode.subState = TITLE_CARD_FORMAT_FAILED;
          g_nTitleMode.selection = 1; /* cancel */
          Titlescreen_PlayUiSound(TITLE_SFX_ERROR);
        }
      }
      break;
    }
    case TITLE_CARD_FORMAT_DONE: {
      PollMemCardSyncRing();

      if (g_nTitleMode.cardStatus[g_nTitleMode.cardPort] != McErrNone ||
          g_nTitleMode.cardStatus[1 - g_nTitleMode.cardPort] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nTitleMode.animTimer >= 8) {
        if (RestartTitleCardCheckOnTriangle()) {
          /* triangle was pushed, RestartTitleCardCheckOnTriangle already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          Titlescreen_PlayUiSound(TITLE_SFX_SELECT);
          g_nTitleMode.subState = TITLE_CARD_READ;
          g_nTitleMode.menuState = 0;
          g_nTitleMode.retryCount = 0;
          g_nTitleMode.selection = 1;
        }
      }

      break;
    }
    case TITLE_CARD_FORMAT_FAILED: {
      PollMemCardSyncRing();

      if (g_nTitleMode.cardStatus[g_nTitleMode.cardPort] != McErrNone ||
          g_nTitleMode.cardStatus[1 - g_nTitleMode.cardPort] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nTitleMode.animTimer >= 8) {
        if (RestartTitleCardCheckOnTriangle()) {
          /* triangle was pushed, RestartTitleCardCheckOnTriangle already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          if (g_nTitleMode.selection == 0) {
            /* retry format */
            g_nTitleMode.subState = TITLE_CARD_FORMATTING;
            MemCardSync(0, (long *)&g_nTitleMode.mcCmd,
                          (long *)&g_nTitleMode.mcResult);
            Titlescreen_PlayUiSound(TITLE_SFX_CARD_ACCESS);
          } else {
            g_nTitleMode.subState = TITLE_CARD_UNUSABLE;
            g_nTitleMode.selection = 0;
            Titlescreen_PlayUiSound(TITLE_SFX_PROMPT);
          }
        }
      }
      break;
    }
    case TITLE_CARD_CREATE_ASK: {
      PollMemCardSyncRing();

      if (g_nTitleMode.cardStatus[g_nTitleMode.cardPort] != McErrNone ||
          g_nTitleMode.cardStatus[1 - g_nTitleMode.cardPort] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nTitleMode.animTimer == 0) {
        Titlescreen_PlayUiSound(TITLE_SFX_PROMPT);
      }

      if (g_nTitleMode.animTimer >= 8) {
        if (RestartTitleCardCheckOnTriangle()) {
          /* triangle was pushed, RestartTitleCardCheckOnTriangle already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          if (g_nTitleMode.selection == 0) {
            g_nTitleMode.subState = TITLE_CARD_CREATING;
            g_nTitleMode.menuState = 0;
            g_nTitleMode.retryCount = 0;
            MemCardSync(0, (long *)&g_nTitleMode.mcCmd,
                          (long *)&g_nTitleMode.mcResult);
            Titlescreen_PlayUiSound(TITLE_SFX_CARD_ACCESS);
          } else {
            g_nTitleMode.subState = TITLE_CARD_UNUSABLE;
            g_nTitleMode.selection = 0;
            Titlescreen_PlayUiSound(TITLE_SFX_PROMPT);
          }
        }
      }

      break;
    }
    case TITLE_CARD_CREATING: {
      int i;
      if (g_nTitleMode.menuState == 0) {
        g_nTitleMode.menuState = 1; /* ok ... */
      } else if (g_nTitleMode.menuState == 1) {
        if (g_nTitleMode.retryCount == 0) {
          g_nTitleMode.mcResult = MemCardCreateFile(
              g_nTitleMode.cardPort << 4, (char *)s_SaveFileName, 1);
        }
        if (g_nTitleMode.mcResult == McErrNone) {
          char *mcs = (char *)g_nTitleMode.file;

          /* copy over the header/icons */
          CopyWords(g_nTitleMode.file, g_abSaveFileHeaderTemplate,
                        sizeof(g_abSaveFileHeaderTemplate));

          g_nSaveSlotLoaded_s.fileTag = ReadRootCounter2();
          g_nTitleMode.file->header.clut[15] = g_nSaveSlotLoaded_s.fileTag;

          /* reset game and create all 3 saves */
          mcs += sizeof(g_abSaveFileHeaderTemplate);
          InitNewGameState();
          for (i = 0; i < 3; ++i) {
            BuildSaveGameBuffer((SaveGameBuffer *)(mcs + i * sizeof(SaveGameBuffer)));
          }
          MemCardWrite(g_nTitleMode.cardPort << 4, (char *)s_SaveFileName,
                        (unsigned long *)g_nTitleMode.file, 0,
                        sizeof(MemCardSaveFile));

          g_nTitleMode.menuState = 2;
        } else if (g_nTitleMode.mcResult == McErrBlockFull) {
          g_nTitleMode.subState = TITLE_CARD_FULL;
          g_nTitleMode.selection = 0;
        } else {
          g_nTitleMode.subState = TITLE_CARD_CREATE_FAILED;
          g_nTitleMode.selection = 0;
        }
      } else if (g_nTitleMode.menuState == 2) {
        /* kick back if MemCardWrite is not complete yet */
        if (MemCardSync(1, (long *)&g_nTitleMode.mcCmd,
                          (long *)&g_nTitleMode.mcResult) == 0)
          break;

        /* write is done, check to see if it was successful */
        if (g_nTitleMode.mcResult != McErrNone) {
          g_nTitleMode.subState = TITLE_CARD_CREATE_FAILED;
          g_nTitleMode.selection = 0;
        } else {
          /* save succeeded, let's re-read it off the card */
          MemCardRead(g_nTitleMode.cardPort << 4, (char *)s_SaveFileName,
                        (unsigned long *)g_nTitleMode.file, 0,
                        sizeof(MemCardSaveFile));
          g_nTitleMode.menuState = 3;
        }
      } else {
        /* make sure the re-read looks good */
        if (g_nTitleMode.mcResult == McErrNone) {
          if (ComputeSaveGameChecksum(
                  (u_char *)&g_nTitleMode.file->saves[0]) ==
                  g_nTitleMode.file->saves[0].checksum &&
              ComputeSaveGameChecksum(
                  (u_char *)&g_nTitleMode.file->saves[1]) ==
                  g_nTitleMode.file->saves[1].checksum &&
              ComputeSaveGameChecksum(
                  (u_char *)&g_nTitleMode.file->saves[2]) ==
                  g_nTitleMode.file->saves[2].checksum) {
            g_nTitleMode.mode = TITLE_MODE_SAVE_SELECT;
            g_nTitleMode.menuState = TITLE_MENU_NEW_OR_LOAD;
            g_nTitleMode.selection = 0;
            g_nSkipGamestateDraw = 1;
            Titlescreen_PlayUiSound(TITLE_SFX_SUCCESS);
          } else {
            if (g_nTitleMode.retryCount < 2) {
              g_nTitleMode.retryCount++;
              g_nTitleMode.menuState = 2;
            } else {
              InitNewGameState();
              if (ComputeSaveGameChecksum(
                      (u_char *)&g_nTitleMode.file->saves[0]) !=
                  g_nTitleMode.file->saves[0].checksum)
                BuildSaveGameBuffer(&g_nTitleMode.file->saves[0]);
              if (ComputeSaveGameChecksum(
                      (u_char *)&g_nTitleMode.file->saves[1]) !=
                  g_nTitleMode.file->saves[1].checksum)
                BuildSaveGameBuffer(&g_nTitleMode.file->saves[1]);
              if (ComputeSaveGameChecksum(
                      (u_char *)&g_nTitleMode.file->saves[2]) !=
                  g_nTitleMode.file->saves[2].checksum)
                BuildSaveGameBuffer(&g_nTitleMode.file->saves[2]);
              g_nTitleMode.mode = TITLE_MODE_SAVE_SELECT;
              g_nTitleMode.menuState = TITLE_MENU_NEW_OR_LOAD;
              g_nTitleMode.selection = 0;
              g_nSkipGamestateDraw = 1;
              Titlescreen_PlayUiSound(TITLE_SFX_SUCCESS);
            }
          }
        } else {
          g_nTitleMode.subState = TITLE_CARD_CREATE_FAILED;
          g_nTitleMode.selection = 0;
        }
      }
      break;
    }
    case TITLE_CARD_FULL: {
      PollMemCardSyncRing();

      if (g_nTitleMode.cardStatus[g_nTitleMode.cardPort] != McErrNone ||
          g_nTitleMode.cardStatus[1 - g_nTitleMode.cardPort] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nTitleMode.animTimer == 0) {
        Titlescreen_PlayUiSound(TITLE_SFX_ERROR);
      }

      if (g_nTitleMode.animTimer >= 8) {
        if (RestartTitleCardCheckOnTriangle()) {
          /* triangle was pushed, RestartTitleCardCheckOnTriangle already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS)) {
          Titlescreen_PlayUiSound(TITLE_SFX_PROMPT);
          g_nTitleMode.subState = TITLE_CARD_UNUSABLE;
          g_nTitleMode.selection = 0;
        }
      }
      break;
    }
    case TITLE_CARD_CREATE_FAILED: {
      PollMemCardSyncRing();

      if (g_nTitleMode.cardStatus[g_nTitleMode.cardPort] != McErrNone ||
          g_nTitleMode.cardStatus[1 - g_nTitleMode.cardPort] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nTitleMode.animTimer == 0) {
        Titlescreen_PlayUiSound(TITLE_SFX_ERROR);
      }

      if (g_nTitleMode.animTimer >= 8) {
        if (RestartTitleCardCheckOnTriangle()) {
          /* triangle was pushed, RestartTitleCardCheckOnTriangle already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          if (g_nTitleMode.selection == 0) {
            g_nTitleMode.subState = TITLE_CARD_CREATING;
            g_nTitleMode.menuState = 0;
            g_nTitleMode.retryCount = 0;
            MemCardSync(0, (long *)&g_nTitleMode.mcCmd,
                          (long *)&g_nTitleMode.mcResult);
            Titlescreen_PlayUiSound(TITLE_SFX_CARD_ACCESS);
          } else {
            g_nTitleMode.subState = TITLE_CARD_UNUSABLE;
            g_nTitleMode.selection = 0;
            Titlescreen_PlayUiSound(TITLE_SFX_PROMPT);
          }
        }
      }
      break;
    }
    case TITLE_CARD_PICK_SLOT: {
      PollMemCardSyncRing();

      if (g_nTitleMode.cardStatus[0] == McErrCardNotExist ||
          g_nTitleMode.cardStatus[0] == McErrNewCard ||
          g_nTitleMode.cardStatus[1] == McErrCardNotExist ||
          g_nTitleMode.cardStatus[1] == McErrNewCard) {
        RELOAD_MEMCARDS;
        break;
      }

      if (g_nTitleMode.animTimer == 0) {
        Titlescreen_PlayUiSound(TITLE_SFX_PROMPT);
      }

      if (g_nTitleMode.animTimer >= 8) {
        if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          g_nTitleMode.cardPort = g_nTitleMode.selection;
          if (g_nTitleMode.needsFormat[g_nTitleMode.selection]) {
            g_nTitleMode.subState = TITLE_CARD_FORMAT_ASK;
            g_nTitleMode.selection = 0;
          } else {
            g_nTitleMode.subState = TITLE_CARD_READ;
            g_nTitleMode.menuState = 0;
            g_nTitleMode.retryCount = 0;
            g_nTitleMode.selection = 1;
            Titlescreen_PlayUiSound(TITLE_SFX_SELECT);
          }
        }
      }
      break;
    }
    }
    break;
  }
  case TITLE_MODE_SAVE_SELECT: {
    /* while not transitioning to the demo, keep checking our card states */
    if (g_nTitleMode.menuState < TITLE_MENU_FADE_OUT) {
      PollMemCardSyncRing();

      if (g_nTitleMode.cardStatus[g_nTitleMode.cardPort] != McErrNone ||
          g_nTitleMode.cardStatus[1 - g_nTitleMode.cardPort] ==
              McErrNewCard) {
        g_nTitleMode.mode = TITLE_MODE_CARD_CHECK;
        g_nTitleMode.subState = TITLE_CARD_PROBE;
        g_nTitleMode.menuState = 0;
        MemCardSync(0, (long *)&g_nTitleMode.mcCmd,
                      (long *)&g_nTitleMode.mcResult);
        break;
      }
    }

    /* keep checking to see if we have any save files */
    g_nTitleMode.apSaves[0] = &g_nTitleMode.file->saves[0];
    g_nTitleMode.apSaves[1] = &g_nTitleMode.file->saves[1];
    g_nTitleMode.apSaves[2] = &g_nTitleMode.file->saves[2];
    if (!g_nTitleMode.apSaves[0]->levelVisited[0] &&
        !g_nTitleMode.apSaves[1]->levelVisited[0] &&
        !g_nTitleMode.apSaves[2]->levelVisited[0]) {
      g_nTitleMode.anySaveUsed = 0;
    } else {
      g_nTitleMode.anySaveUsed = 1;
    }

    if (g_nTitleMode.menuState == TITLE_MENU_ROUTE) {
      if (g_nTitleMode.savedSelection == 0) {
        g_nTitleMode.menuState = TITLE_MENU_NEW_SLOT;
        if (!g_nTitleMode.apSaves[0]->levelVisited[0]) {
          g_nTitleMode.selection = 0;
        } else if (!g_nTitleMode.apSaves[1]->levelVisited[0]) {
          g_nTitleMode.selection = 1;
        } else if (!g_nTitleMode.apSaves[2]->levelVisited[0]) {
          g_nTitleMode.selection = 2;
        } else {
          g_nTitleMode.selection = 0;
        }
      } else if (g_nTitleMode.anySaveUsed) {
        g_nTitleMode.menuState = TITLE_MENU_LOAD_SLOT;
        if (g_nTitleMode.apSaves[0]->levelVisited[0]) {
          g_nTitleMode.selection = 0;
        } else if (g_nTitleMode.apSaves[1]->levelVisited[0]) {
          g_nTitleMode.selection = 1;
        } else {
          g_nTitleMode.selection = 2;
        }
      } else {
        g_nTitleMode.selection = 1;
        g_nTitleMode.menuState = TITLE_MENU_NEW_OR_LOAD;
      }
    } else if (g_nTitleMode.menuState == TITLE_MENU_NEW_SLOT) {
      if (g_nTitleMode.animTimer >= 8) {
        if (g_dwPadPressed.pressed & (PAD_DOWN | PAD_TRIANGLE)) {
          Titlescreen_PlayUiSound(TITLE_SFX_SUCCESS);
          g_nTitleMode.menuState = TITLE_MENU_NEW_OR_LOAD;
          g_nTitleMode.selection = 0;
        } else if (g_dwPadPressed.pressed & PAD_LEFT) {
          g_nTitleMode.animTimer = 16;
          g_nTitleMode.selection -= 1;
          if (g_nTitleMode.selection < 0)
            g_nTitleMode.selection = 2;
          Titlescreen_PlayUiSound(TITLE_SFX_MOVE);
        } else if (g_dwPadPressed.pressed & PAD_RIGHT) {
          g_nTitleMode.animTimer = 16;
          g_nTitleMode.selection += 1;
          if (g_nTitleMode.selection > 2)
            g_nTitleMode.selection = 0;
          Titlescreen_PlayUiSound(TITLE_SFX_MOVE);
        } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS)) {
          if (g_nTitleMode.apSaves[g_nTitleMode.selection]
                  ->levelVisited[0]) {
            Titlescreen_PlayUiSound(TITLE_SFX_PROMPT);
            g_nTitleMode.menuState = TITLE_MENU_OVERWRITE;
            g_nTitleMode.yesNoChoice = 1;
          } else {
            InitNewGameState();
            START_GAME(TITLE_EXIT_NEW_GAME)
          }
        }
      }
    } else if (g_nTitleMode.menuState == TITLE_MENU_OVERWRITE) {
      if (g_nTitleMode.animTimer >= 8) {
        if ((g_dwPadPressed.pressed & (PAD_RIGHT | PAD_LEFT))) {
          g_nTitleMode.animTimer = 16;
          g_nTitleMode.yesNoChoice = 1 - g_nTitleMode.yesNoChoice;
          Titlescreen_PlayUiSound(TITLE_SFX_MOVE);
        } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS | PAD_TRIANGLE)) {
          if (g_nTitleMode.yesNoChoice == 1 ||
              (g_dwPadPressed.pressed & PAD_TRIANGLE) != 0) {
            g_nTitleMode.menuState = TITLE_MENU_NEW_SLOT;
            Titlescreen_PlayUiSound(TITLE_SFX_ERROR);
          } else {
            InitNewGameState();
            START_GAME(TITLE_EXIT_NEW_GAME)
          }
        }
      }
    } else if (g_nTitleMode.menuState == TITLE_MENU_LOAD_SLOT) {
      if (g_nTitleMode.animTimer >= 8) {
        if (g_dwPadPressed.pressed & (PAD_DOWN | PAD_TRIANGLE)) {
          Titlescreen_PlayUiSound(TITLE_SFX_SUCCESS);
          g_nTitleMode.menuState = TITLE_MENU_NEW_OR_LOAD;
          g_nTitleMode.selection = 1;
        } else if (g_dwPadPressed.pressed & PAD_LEFT) {
          int i;
          for (i = g_nTitleMode.selection - 1; i >= 0; --i) {
            if (g_nTitleMode.apSaves[i]->levelVisited[0]) {
              g_nTitleMode.animTimer = 16;
              break;
            }
          }
          if (i >= 0) {
            g_nTitleMode.selection = i;
            Titlescreen_PlayUiSound(TITLE_SFX_MOVE);
          }
        } else if (g_dwPadPressed.pressed & PAD_RIGHT) {
          int i;
          for (i = g_nTitleMode.selection + 1; i < 3; ++i) {
            if (g_nTitleMode.apSaves[i]->levelVisited[0]) {
              g_nTitleMode.animTimer = 16;
              break;
            }
          }
          if (i < 3) {
            g_nTitleMode.selection = i;
            Titlescreen_PlayUiSound(TITLE_SFX_MOVE);
          }
        } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS)) {
          g_nSparxHealth = SPARX_GOLD;
          g_nSparxFoodCount = 0;
          g_nSparxHealthStash = -1;
          ApplySaveGameBuffer(g_nTitleMode.apSaves[g_nTitleMode.selection]);
          START_GAME(TITLE_EXIT_LOADED_GAME)
        }
      }
    } else if (g_nTitleMode.menuState == TITLE_MENU_NEW_OR_LOAD) {
      if (g_nTitleMode.animTimer >= 8) {
        if (RestartTitleCardCheckOnTriangle()) {
          /* triangle was pushed, RestartTitleCardCheckOnTriangle already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_RIGHT | PAD_LEFT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_UP | PAD_START | PAD_CROSS)) {
          if (g_nTitleMode.selection == 1 && !g_nTitleMode.anySaveUsed) {
            g_nTitleMode.animTimer = 16;
            Titlescreen_PlayUiSound(TITLE_SFX_ERROR);
          } else {
            g_nTitleMode.menuState = TITLE_MENU_ROUTE;
            g_nSkipGamestateDraw = 1;
            g_nTitleMode.savedSelection = g_nTitleMode.selection;
            Titlescreen_PlayUiSound(TITLE_SFX_SELECT);
          }
        }
      }
    } else if (g_nTitleMode.menuState == TITLE_MENU_FADE_OUT) {
      if (g_nTitleMode.animTimer == 0) {
        Titlescreen_PlayUiSound(TITLE_SFX_FADE_OUT);
      }

      g_nScreenFadeLevel = g_nTitleMode.animTimer + 1;
      if (g_nScreenFadeLevel > 15)
        g_nScreenFadeLevel = 15;

      if (g_nTitleMode.animTimer >= 16) {
        RECT rc;
        MemCardCloseSession(); /* MemCardStop */
        g_nTitleMode.mode = TITLE_MODE_STORY_INTRO;
        g_nTitleMode.menuState = 0;
        setRECT(&rc, 0, 0, 512, 480);
        ClearImage(&rc, 0, 0, 0);
        DrawSync(0); /* DrawSync */
      }
    }
    break;
  }
  }

  RotateLightVectorXZ(3); /* SpecularUpdate */
  if (g_dwPadPressed.padType >= PAD_TYPE_DIGITAL && (u_short)g_dwPadPressed.held != 0) {
    g_nTitleMode.idleFrames = 0;
  } else {
    g_nTitleMode.idleFrames++;
  }

  if (g_nTitleMode.mode < TITLE_MODE_SAVE_SELECT) {
    if (g_nTitleMode.subState != old_state)
      g_nTitleMode.animTimer = 0;
    else
      g_nTitleMode.animTimer = g_nTitleMode.animTimer + 1;
  } else {
    if (g_nTitleMode.menuState != old_state)
      g_nTitleMode.animTimer = 0;
    else
      g_nTitleMode.animTimer = g_nTitleMode.animTimer + 1;
  }
}

#undef SWAP_MENU_SELECTION
#undef RELOAD_MEMCARDS
#undef START_GAME

/* Titlescreen_Draw (0x8007CEE4, titlescreen overlay).
 *
 * Per-frame draw for the whole title screen: the mode/substate-driven sprite
 * overlays (logo fly-in, memory-card menus, save-file picker), then the shared
 * frame tail — background color, actor/environment/cyclorama submission, fade,
 * vsync pacing, and the OT flush.
 *
 * Its substate switch owns the jump table at rodata 0x5C (jtbl_8007AA94),
 * emitted by this object's .rodata and slot-placed there.
 *
 * Verified byte-identical inside the relinked titlescreen.ovl.
 */


#define DRAW_OPTIONS(op1, op2)                                                 \
  if (g_nTitleMode.animTimer > 7) {                                              \
    Titlescreen_EmitSpriteQuad(128, 88, op1,                                        \
                          (g_nTitleMode.selection == 0 &&                 \
                           ((g_nTitleMode.animTimer & 0xF) < 8))                 \
                              ? TITLE_COLOR_HIGHLIGHT                                \
                              : TITLE_COLOR_NORMAL);                                \
    Titlescreen_EmitSpriteQuad(256, 88, op2,                                        \
                          (g_nTitleMode.selection == 1 &&                 \
                           ((g_nTitleMode.animTimer & 0xF) < 8))                 \
                              ? TITLE_COLOR_HIGHLIGHT                                \
                              : TITLE_COLOR_NORMAL);                                \
  }

void Titlescreen_Draw(void) {

  switch (g_nTitleMode.mode) {
  case TITLE_MODE_ATTRACT: {

    if (g_nTitleMode.subState == TITLE_ATTRACT_FLY_IN) {
      /* spyro landed, logo incoming */
      if (g_pPathTableHead->frame >= 1170)
        Titlescreen_EmitSpriteQuad(128, g_abTitleSlideInCurve[g_nTitleMode.animTimer] - 128,
                              TITLE_SPR_LOGO, TITLE_COLOR_NORMAL);
    } else if (g_nTitleMode.subState == TITLE_ATTRACT_PRESS_START) {
      Titlescreen_EmitSpriteQuad(128, 0, TITLE_SPR_LOGO, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(192, 210, TITLE_SPR_PRESS_START,
                            ((g_nTitleMode.animTimer & 0xF) < 8) * TITLE_COLOR_PROMPT);
    } else if (g_nTitleMode.subState == TITLE_ATTRACT_LOGO_OUT) {
      if (g_nTitleMode.animTimer < 16) {
        Titlescreen_EmitSpriteQuad(128, g_abTitleSlideOutCurve[g_nTitleMode.animTimer] - 128,
                              TITLE_SPR_LOGO, TITLE_COLOR_NORMAL);
      } else {
        Titlescreen_EmitSpriteQuad(108, g_abTitleSlideInCurve[g_nTitleMode.animTimer - 16] - 119,
                              TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
        Titlescreen_EmitSpriteQuad(255, g_abTitleSlideInCurve[g_nTitleMode.animTimer - 16] - 119,
                              -TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
      }
    }
    break;
  }
  case TITLE_MODE_CARD_CHECK: {
    /* draw main menu decorative borders (left, then mirror right) */
    Titlescreen_EmitSpriteQuad(108, 9, TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
    Titlescreen_EmitSpriteQuad(255, 9, -TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);

    switch (g_nTitleMode.subState) {
    case TITLE_CARD_PROBE:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_CHECKING,
                            TITLE_COLOR_NORMAL); /* accessing mem card */
      break;
    case TITLE_CARD_READ: /* memcard slot selected, checking */
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_CHECKING, TITLE_COLOR_NORMAL);
      if (g_nTitleMode.selection != 0) {
        Titlescreen_EmitSpriteQuad(128, 106,
                              g_nTitleMode.cardPort +
                                  TITLE_SPR_CARD_SLOT_LABEL,
                              TITLE_COLOR_NORMAL);
      }
      break;
    case TITLE_CARD_NONE:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_NO_CARD, TITLE_COLOR_NORMAL);
      DRAW_OPTIONS(TITLE_SPR_OPT_NO_SAVE, TITLE_SPR_OPT_RETRY);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_UNUSABLE:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_CARD_UNUSABLE,
                            TITLE_COLOR_NORMAL);
      DRAW_OPTIONS(TITLE_SPR_OPT_NO_SAVE, TITLE_SPR_OPT_RETRY);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_NO_SAVE_WARNING:
      Titlescreen_EmitSpriteQuad(128, 22, TITLE_SPR_MSG_WARNING, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 38, TITLE_SPR_MSG_NO_SAVE_L2,
                            TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 54, TITLE_SPR_MSG_NO_SAVE_L3,
                            TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 70, TITLE_SPR_MSG_NO_SAVE_L4,
                            TITLE_COLOR_NORMAL);
      DRAW_OPTIONS(TITLE_SPR_OPT_NO_SAVE, TITLE_SPR_OPT_CANCEL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_FORMAT_ASK:
    case TITLE_CARD_FORMAT_CONFIRM:
      if (g_nTitleMode.subState == TITLE_CARD_FORMAT_ASK) {
        Titlescreen_EmitSpriteQuad(128, 30,
                              g_nTitleMode.cardPort +
                                  TITLE_SPR_MSG_CARD_IN_SLOT,
                              TITLE_COLOR_NORMAL);
        Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_UNFORMATTED_L1, TITLE_COLOR_NORMAL);
        Titlescreen_EmitSpriteQuad(128, 62, TITLE_SPR_MSG_UNFORMATTED_L2, TITLE_COLOR_NORMAL);
      } else {
        Titlescreen_EmitSpriteQuad(128, 30, TITLE_SPR_MSG_WARNING, TITLE_COLOR_NORMAL);
        Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_FORMAT_SURE_L1,
                              TITLE_COLOR_NORMAL);
        Titlescreen_EmitSpriteQuad(128, 62, TITLE_SPR_MSG_FORMAT_SURE_L2,
                              TITLE_COLOR_NORMAL);
      }
      DRAW_OPTIONS(TITLE_SPR_OPT_FORMAT, TITLE_SPR_OPT_CANCEL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_FORMATTING:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_FORMATTING, TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_FORMAT_DONE:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_FORMAT_DONE, TITLE_COLOR_NORMAL);
      if (g_nTitleMode.animTimer > 7) {
        Titlescreen_EmitSpriteQuad(128, 88, TITLE_SPR_OPT_CONTINUE,
                              (g_nTitleMode.animTimer & 0xf) < 8
                                  ? TITLE_COLOR_HIGHLIGHT
                                  : TITLE_COLOR_NORMAL);
      }
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_FORMAT_FAILED:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_FORMAT_FAILED, TITLE_COLOR_NORMAL);
      DRAW_OPTIONS(TITLE_SPR_OPT_RETRY, TITLE_SPR_OPT_CANCEL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_CREATE_ASK:
      Titlescreen_EmitSpriteQuad(128, 30,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_MSG_CARD_IN_SLOT,
                            TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_NO_FILE_L1,
                            TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 62, TITLE_SPR_MSG_NO_FILE_L2,
                            TITLE_COLOR_NORMAL);

      DRAW_OPTIONS(TITLE_SPR_OPT_CREATE, TITLE_SPR_OPT_CANCEL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_CREATING:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_CREATING, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_FULL:
      Titlescreen_EmitSpriteQuad(128, 22,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_MSG_CARD_IN_SLOT,
                            0);
      Titlescreen_EmitSpriteQuad(128, 38, TITLE_SPR_MSG_CARD_FULL_L1, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 54, TITLE_SPR_MSG_CARD_FULL_L2,
                            TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 70, TITLE_SPR_MSG_CARD_FULL_L3,
                            TITLE_COLOR_NORMAL);
      if (g_nTitleMode.animTimer > 7) {
        Titlescreen_EmitSpriteQuad(128, 88, TITLE_SPR_OPT_CONTINUE,
                              (g_nTitleMode.animTimer & 0xF) < 8);
      }
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    case TITLE_CARD_CREATE_FAILED:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_CREATE_FAILED,
                            TITLE_COLOR_NORMAL);
      DRAW_OPTIONS(TITLE_SPR_OPT_RETRY, TITLE_SPR_OPT_CANCEL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
      break;
    /* no case 14 */
    case TITLE_CARD_PICK_SLOT:
      Titlescreen_EmitSpriteQuad(128, 46, TITLE_SPR_MSG_PICK_CARD, TITLE_COLOR_NORMAL);
      DRAW_OPTIONS(TITLE_SPR_OPT_SLOT1, TITLE_SPR_OPT_SLOT2);
      break;
    default:
      break;
    }
    break;
  }
  case TITLE_MODE_SAVE_SELECT: {
    int i;
    int j;
    int dragon_count;
    if (g_nTitleMode.menuState < TITLE_MENU_FADE_OUT) {
      Titlescreen_EmitSpriteQuad(108, 9, TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(255, 9, -TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
      if (g_nTitleMode.menuState > TITLE_MENU_ROUTE) {
        for (i = 0; i < 3; ++i) {
          if (!g_nTitleMode.apSaves[i]->levelVisited[0]) {
            int color = 0;
            if ((g_nTitleMode.menuState < TITLE_MENU_LOAD_SLOT &&
                 g_nTitleMode.selection != i) ||
                g_nTitleMode.menuState - 3u < 2) {
              color = TITLE_COLOR_DIMMED;
            }
            Titlescreen_EmitSpriteQuad(140 + 80 * i, 38, TITLE_SPR_EMPTY_SLOT, color);
          } else {
            int color = 0;
            if ((g_nTitleMode.menuState < TITLE_MENU_NEW_OR_LOAD &&
                 g_nTitleMode.selection != i) ||
                g_nTitleMode.menuState == TITLE_MENU_NEW_OR_LOAD) {
              color = TITLE_COLOR_DIMMED;
            }
            Titlescreen_EmitSpriteQuad(140 + 80 * i, 38,
                                  g_nTitleMode.apSaves[i]->levelId / 10 -
                                      1 + TITLE_SPR_WORLD_NAME,
                                  color);
            Titlescreen_EmitSpriteQuad(184 + 80 * i, 22, TITLE_SPR_DRAGON_ICON,
                                  color);

            for (j = 0, dragon_count = 0; j < TOTAL_LEVEL_COUNT; ++j) {
              dragon_count += g_nTitleMode.apSaves[i]->levelDragons[j];
            }

            /* dragon count ones digit */
            Titlescreen_EmitSpriteQuad(168 + 80 * i, 22,
                                  dragon_count % 10 + TITLE_SPR_DIGIT0,
                                  color);
            if (dragon_count > 9) {
              /* dragon count tens digit */
              Titlescreen_EmitSpriteQuad(152 + 80 * i, 22,
                                    dragon_count / 10 + TITLE_SPR_DIGIT0,
                                    color);
            }
          }
        }
      }
    } else {
      Titlescreen_EmitSpriteQuad(108, g_abTitleSlideOutCurve[g_nTitleMode.animTimer] - 119,
                            TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(255, g_abTitleSlideOutCurve[g_nTitleMode.animTimer] - 119,
                            -TITLE_SPR_BORDER, TITLE_COLOR_NORMAL);
    }

    if (g_nTitleMode.menuState == TITLE_MENU_NEW_SLOT) {
      if ((g_nTitleMode.animTimer & 0xF) < 8) {
        Titlescreen_EmitSpriteQuad(g_nTitleMode.selection * 80 + 140, 38,
                              TITLE_SPR_SLOT_CURSOR, TITLE_COLOR_NORMAL);
      }
      Titlescreen_EmitSpriteQuad(128, 88, TITLE_SPR_OPT_NEW_GAME, TITLE_COLOR_HIGHLIGHT);
      Titlescreen_EmitSpriteQuad(256, 88, TITLE_SPR_OPT_LOAD_GAME, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 106, TITLE_SPR_MSG_PICK_NEW_SLOT,
                            TITLE_COLOR_NORMAL);
    } else if (g_nTitleMode.menuState == TITLE_MENU_OVERWRITE) {
      Titlescreen_EmitSpriteQuad(g_nTitleMode.selection * 80 + 140, 38,
                            TITLE_SPR_SLOT_MARK, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(g_nTitleMode.selection * 80 + 140, 38,
                            TITLE_SPR_SLOT_CURSOR, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 88, TITLE_SPR_OPT_OVERWRITE,
                            (g_nTitleMode.yesNoChoice == 0 &&
                             ((g_nTitleMode.animTimer & 0xF) < 8))
                                ? TITLE_COLOR_HIGHLIGHT
                                : TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(256, 88, TITLE_SPR_OPT_CANCEL,
                            (g_nTitleMode.yesNoChoice == 1 &&
                             ((g_nTitleMode.animTimer & 0xF) < 8))
                                ? TITLE_COLOR_HIGHLIGHT
                                : TITLE_COLOR_NORMAL);
    } else if (g_nTitleMode.menuState == TITLE_MENU_LOAD_SLOT) {
      if ((g_nTitleMode.animTimer & 0xF) < 8) {
        Titlescreen_EmitSpriteQuad(g_nTitleMode.selection * 80 + 140, 38,
                              TITLE_SPR_SLOT_CURSOR, TITLE_COLOR_NORMAL);
      }
      Titlescreen_EmitSpriteQuad(128, 88, TITLE_SPR_OPT_NEW_GAME, TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(256, 88, TITLE_SPR_OPT_LOAD_GAME, TITLE_COLOR_HIGHLIGHT);
      Titlescreen_EmitSpriteQuad(128, 106, TITLE_SPR_MSG_PICK_LOAD_SLOT,
                            TITLE_COLOR_NORMAL);
    } else if (g_nTitleMode.menuState == TITLE_MENU_NEW_OR_LOAD) {
      Titlescreen_EmitSpriteQuad(128, 88, TITLE_SPR_OPT_NEW_GAME,
                            (g_nTitleMode.selection == 0 &&
                             ((g_nTitleMode.animTimer & 0xF) < 8))
                                ? TITLE_COLOR_HIGHLIGHT
                                : TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(256, 88, TITLE_SPR_OPT_LOAD_GAME,
                            (g_nTitleMode.selection == 1 &&
                             ((g_nTitleMode.animTimer & 0xF) < 8))
                                ? TITLE_COLOR_HIGHLIGHT
                                : TITLE_COLOR_NORMAL);
      Titlescreen_EmitSpriteQuad(128, 106,
                            g_nTitleMode.cardPort +
                                TITLE_SPR_CARD_SLOT_LABEL,
                            TITLE_COLOR_NORMAL);
    }

    break;
  }
  }

  /* setRGB0 of both draw envs from the cyclorama background color */
  g_bFrameDrawEnv0BgR.r = g_dwWorldFogColor.r, g_bFrameDrawEnv0BgR.g = g_dwWorldFogColor.g,
  g_bFrameDrawEnv0BgR.b = g_dwWorldFogColor.b;
  g_bFrameDrawEnv1BgR.r = g_dwWorldFogColor.r, g_bFrameDrawEnv1BgR.g = g_dwWorldFogColor.g,
  g_bFrameDrawEnv1BgR.b = g_dwWorldFogColor.b;

  /* draw the mobys */
  BuildRenderEntityLists();
  BuildActorDrawList();
  FillWord(g_apMainDrawEntityList, 0, FRAME_ARENA_LIST_WORDS);
  EmitActorDrawList();

  /* draw the environment */
  FillWord(g_apMainDrawEntityList, 0, 0x1C00);
  g_nFarPlaneDrawDistance = 0x1c000;
  RenderWorldChunks(-1);

  /* draw the cyclorama (view matrix, then projection matrix just below it) */
  EmitSkyMesh(-1, g_anCameraViewMtx,
                          g_anCameraViewMtx - 0x14 /* g_anWorldToCameraRotMtx */);

  /* overlay the fade */
  if (g_nScreenFadeLevel) {
    DrawFullscreenTint(2, g_nScreenFadeLevel << 4, g_nScreenFadeLevel << 4, g_nScreenFadeLevel << 4);
  }

  DrawSync(0);

  VSync(0);

  g_nVsyncFramePaceAnchor.pre = VSync(-1);

  while (g_nVsyncFramePaceAnchor.pre - g_nVsyncFramePaceAnchor.post < 2) {
    VSync(0);
    g_nVsyncFramePaceAnchor.pre = VSync(-1);
  }

  g_nVsyncFramePaceAnchor.post = VSync(-1);

  PutDispEnv(&g_pActiveFrameDrawEnv->dispEnv);
  PutDrawEnv(&g_pActiveFrameDrawEnv->drawEnv);
  DrawOTag(LinkOTPrimitives(0x800));
}

#undef DRAW_OPTIONS
