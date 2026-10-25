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

#include "titlescreen.h"

static const char s_SaveFileName[20] = "BASCUS-94228SPYRO";

#define SWAP_MENU_SELECTION                                                    \
  g_nGamestate0dMode.animTimer = 16;                                                   \
  g_nGamestate0dMode.selection = 1 - g_nGamestate0dMode.selection;               \
  Titlescreen_PlayUiSound(0);

#define RELOAD_MEMCARDS                                                        \
  g_nGamestate0dMode.subState = SubState_0x00;                            \
  g_nGamestate0dMode.menuState = 0;                                                      \
  MemCardSync(0, (long *)&g_nGamestate0dMode.mcCmd,                    \
                (long *)&g_nGamestate0dMode.mcResult);

#define START_DEMO(DEMO_TYPE)                                                  \
  g_nGamestate0dMode.menuState = 5;                                                      \
  g_nGamestate0dMode.demoType = DEMO_TYPE;                                           \
  g_nSaveSlotLoaded_s.fileTag = g_nGamestate0dMode.file->header.clut[15];         \
  g_nSaveSlotLoaded_s.loaded = 1;                                              \
  g_nSaveSlotLoaded_s.cardPort = g_nGamestate0dMode.cardPort << 4;                \
  g_nSaveSlotLoaded_s.slot = g_nGamestate0dMode.selection;                           \
  MemCardSync(0, (long *)&g_nGamestate0dMode.mcCmd,                    \
                (long *)&g_nGamestate0dMode.mcResult);

void Titlescreen_Update(void) {
  int old_state =
      g_nGamestate0dMode.mode < 2 ? g_nGamestate0dMode.subState : g_nGamestate0dMode.menuState;

  g_pPathTableHead->introFrame += 2;

  if (g_pPathTableHead->introFrame < 1100) {
    g_nGamestate0dMode.idleFrames = 0;
    if (g_nGamestate0dMode.subState == 2 && g_pPathTableHead->introFrame >= 300 &&
        (g_dwPadPressed.held & (PAD_START | PAD_CROSS)) != 0) {
      StopSoundVoiceByIndex(g_bDragonRescueSfxVoiceMarker, 4); /* StopSound(voice) */
      g_pPathTableHead->introFrame = 1170;
    }
  } else {
    if (g_pPathTableHead->introFrame == 1344 && (GetRandomU32() & 3) != 0) {
      g_pPathTableHead->introFrame = 1172;
    } else if (g_pPathTableHead->introFrame >= 1530) {
      g_pPathTableHead->introFrame = 1172;
    }
  }

  if (g_nGenericCountdown)
    --g_nGenericCountdown;

  AdvanceGemCutsceneFrame(g_pPathTableHead->introFrame);

  g_nCameraEulerRoll.x = 0;
  g_nCameraEulerRoll.y += 16;

  switch (g_nGamestate0dMode.mode) {
  case Mode_0x00: {
    if (g_nGamestate0dMode.subState < 2) {
      /* kick out while we're still loading from the CD */
      RestartStalledCdRead();
      if (g_nCdBaseLba.readInFlight != 0 || CallCdSync(1, 0) != 2 ||
          (g_nCurrentMusicTrack.streamStatus & 0x40) == 0)
        break;

      if (g_nGamestate0dMode.subState == 0) {
        /* kick off loading titlescreen data from the CD */
        register int cd asm("$4");
        register int t asm("$7");
        cd = *((volatile int *) &g_nCdBaseLba.baseLba);
        t = g_nTitleSceneSectorOffset;
        StartCdReadAsync(cd, g_pRenderScratchRegionBase - 0x40000, 0x40000,
                      t, 600);
        g_nGamestate0dMode.subState = 1;
      } else if (g_nGamestate0dMode.subState == 1) {
        /* once loaded, initialize music and upload the sprite sheet to vram */
        RECT rc;
        g_nCurrentMusicTrack.track = 20;
        g_nCurrentMusicTrack.streamTimeout = 155520000;
        HandleMusicCommand(20, 1);
        setRECT(&rc, 512, 256, 512, 256);
        LoadImage(&rc, g_pRenderScratchRegionBase - 0x40000);
        g_nGamestate0dMode.subState = 2;
      }
    } else if (g_nGamestate0dMode.subState == 2) {
      /* titlescreen sequence is playing, just waiting for a bit */
      if (g_pPathTableHead->introFrame < 1169) {
        g_nGamestate0dMode.animTimer = 0;
      } else {
        if (g_nGamestate0dMode.animTimer == 1) {
          Titlescreen_PlayUiSound(6);
        }
        if (g_nGamestate0dMode.animTimer >= 16)
          g_nGamestate0dMode.subState = 3;
      }
    } else if (g_nGamestate0dMode.subState == 3) {
      /* ready for button inputs or to time out to show the demo */
      if ((g_dwPadPressed.held & (PAD_L1 | PAD_TRIANGLE)) ==
          (PAD_L1 | PAD_TRIANGLE)) {
        StopAllSoundExceptMask(0); /* stop all sounds */
        Titlescreen_PlayUiSound(9);
        TickSpuPerFrame();
        while (g_nCdBaseLba.readInFlight != 0 || CallCdSync(1, 0) != 2 ||
               (g_nCurrentMusicTrack.streamStatus & 0x40) == 0) {
          RestartStalledCdRead();
          TickActiveSoundVoices(); /* update sounds */
          TickCdMusicStream();
        }
        LaunchCrashDemo(); /* crash-demo launch */
      } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS)) {
        g_nGamestate0dMode.subState = 4;
        Titlescreen_PlayUiSound(7);
      } else if (g_nGamestate0dMode.idleFrames >= 990) {
        g_nGamestate0dMode.subState = 5;
      }
    } else if (g_nGamestate0dMode.subState == 4) {
      /* animate out the logo, move to mem card screen */
      if (g_nGamestate0dMode.animTimer == 16) {
        Titlescreen_PlayUiSound(8);
      }
      if (g_nGamestate0dMode.animTimer >= 32) {
        g_nGamestate0dMode.mode = Mode_0x01;
        g_nGamestate0dMode.subState = SubState_0x00;
        g_nGamestate0dMode.menuState = 0;
        MemCardOpenSession(); /* MemCardStart */
      }
    } else if (g_nGamestate0dMode.subState == 5) {
      /* timed out: fade out for 16 ticks and then move to demo mode */
      if (g_nGamestate0dMode.animTimer == 0) {
        Titlescreen_PlayUiSound(5);
      }
      g_nGenericCountdown = g_nGamestate0dMode.animTimer;
      if (g_nGenericCountdown > 15)
        g_nGenericCountdown = 15;
      if (g_nGamestate0dMode.animTimer >= 16) {
        RECT rc;
        g_nGamestate0dMode.mode = Mode_0x03;
        g_nGamestate0dMode.menuState = 0;
        g_nGamestate0dMode.demoType = 2;
        setRECT(&rc, 0, 0, 512, 480);
        ClearImage(&rc, 0, 0, 0);
        DrawSync(0); /* DrawSync */
      }
    }
    break;
  }
  case Mode_0x01: {
    switch (g_nGamestate0dMode.subState) {
    case SubState_0x00: {
      /* wait for async mem card function to finish */
      if (MemCardSync(1, (long *)&g_nGamestate0dMode.mcCmd,
                        (long *)&g_nGamestate0dMode.mcResult) == 0)
        break;

      if (g_nGamestate0dMode.menuState == 0) {
        /* try to ping port A for a mem card */
        Titlescreen_PlayUiSound(2);
        MemCardLoad(0 /* port A card 0 */);
        g_nGamestate0dMode.menuState = 1;
      } else if (g_nGamestate0dMode.menuState == 1) {
        /* pull memcard state for port A */
        g_nGamestate0dMode.cardStatus[0] = g_nGamestate0dMode.mcResult;

        if (g_nGamestate0dMode.cardStatus[0] == McErrNewCard)
          g_nGamestate0dMode.cardStatus[0] = McErrNone;
        if (g_nGamestate0dMode.cardStatus[0] == McErrNotFormat) {
          g_nGamestate0dMode.cardStatus[0] = McErrNone;
          g_nGamestate0dMode.needsFormat[0] = 1;
        } else
          g_nGamestate0dMode.needsFormat[0] = 0;

        /* now go and ping port B */
        MemCardLoad(0x10 /* port B card 0 */);
        g_nGamestate0dMode.menuState = 2;
      } else {
        /* pull memcard state for port B */
        g_nGamestate0dMode.cardStatus[1] = g_nGamestate0dMode.mcResult;

        if (g_nGamestate0dMode.cardStatus[1] == McErrNewCard)
          g_nGamestate0dMode.cardStatus[1] = McErrNone;
        if (g_nGamestate0dMode.cardStatus[1] == McErrNotFormat) {
          g_nGamestate0dMode.cardStatus[1] = McErrNone;
          g_nGamestate0dMode.needsFormat[1] = 1;
        } else {
          g_nGamestate0dMode.needsFormat[1] = 0;
        }

        /* figure out what screen to go to for our A/B memcard states */
        if (g_nGamestate0dMode.cardStatus[0] == McErrNone &&
            g_nGamestate0dMode.cardStatus[1] == McErrNone) {
          /* both ports have a valid, formatted memcard, go to select menu */
          g_nGamestate0dMode.subState = SubState_0x0F;
          g_nGamestate0dMode.selection = 0;
        } else if (g_nGamestate0dMode.cardStatus[0] == McErrNone &&
                   !g_nGamestate0dMode.needsFormat[0]) {
          /* port A is valid and doesn't need formatting: select it */
          g_nGamestate0dMode.cardPort = 0;
          g_nGamestate0dMode.subState = SubState_0x01;
          g_nGamestate0dMode.menuState = 0;
          g_nGamestate0dMode.retryCount = 0;
          g_nGamestate0dMode.selection = 0;
        } else if (g_nGamestate0dMode.cardStatus[1] == McErrNone &&
                   !g_nGamestate0dMode.needsFormat[1]) {
          /* port B is valid and doesn't need formatting: select it */
          g_nGamestate0dMode.cardPort = 1;
          g_nGamestate0dMode.subState = SubState_0x01;
          g_nGamestate0dMode.menuState = 0;
          g_nGamestate0dMode.retryCount = 0;
          g_nGamestate0dMode.selection = 0;
        } else if (g_nGamestate0dMode.cardStatus[0] == McErrNone &&
                   g_nGamestate0dMode.needsFormat[0]) {
          /* port A is valid but needs a format, ask if that's ok */
          g_nGamestate0dMode.cardPort = 0;
          g_nGamestate0dMode.subState = SubState_0x05;
          g_nGamestate0dMode.selection = 0;
        } else if (g_nGamestate0dMode.cardStatus[1] == McErrNone &&
                   g_nGamestate0dMode.needsFormat[1]) {
          /* port B is valid but needs a format, ask if that's ok */
          g_nGamestate0dMode.cardPort = 1;
          g_nGamestate0dMode.subState = SubState_0x05;
          g_nGamestate0dMode.selection = 0;
        } else {
          /* no valid cards found */
          g_nGamestate0dMode.cardPort = -1;
          g_nGamestate0dMode.subState = SubState_0x02;
          g_nGamestate0dMode.selection = 0;
        }
      }
      break;
    }
    case SubState_0x01: {
      /* wait for async mem card function to finish */
      if (MemCardSync(1, (long *)&g_nGamestate0dMode.mcCmd,
                        (long *)&g_nGamestate0dMode.mcResult) == 0)
        break;

      if (g_nGamestate0dMode.menuState == 0) {
        /* read save file from the selected card */
        MemCardRead(g_nGamestate0dMode.cardPort << 4, (char *)s_SaveFileName,
                      (unsigned long *)g_nGamestate0dMode.file, 0,
                      sizeof(Struct_0x1A00));
        g_nGamestate0dMode.menuState = 1;
      } else if (g_nGamestate0dMode.menuState == 1) {
        /* save file has been read */
        if (g_nGamestate0dMode.mcResult == McErrNone) {
          /* no errors reading, check to see if data is intact */
          if (ComputeSaveGameChecksum(
                  (u_char *)&g_nGamestate0dMode.file->saves[0]) ==
                  g_nGamestate0dMode.file->saves[0].checksum &&
              ComputeSaveGameChecksum(
                  (u_char *)&g_nGamestate0dMode.file->saves[1]) ==
                  g_nGamestate0dMode.file->saves[1].checksum &&
              ComputeSaveGameChecksum(
                  (u_char *)&g_nGamestate0dMode.file->saves[2]) ==
                  g_nGamestate0dMode.file->saves[2].checksum) {
            /* save checksums matched, go to save file picker */
            g_nGamestate0dMode.mode = Mode_0x02;
            g_nGamestate0dMode.menuState = 4;
            g_nGamestate0dMode.selection = 1;
            g_nGamePaused = 1;
            Titlescreen_PlayUiSound(9);
          } else if (g_nGamestate0dMode.retryCount < 2) {
            /* save checksums didn't match, retry the read up to 2 times */
            g_nGamestate0dMode.retryCount++;
            g_nGamestate0dMode.menuState = 0;
          } else {
            /* retries didn't work, recreate the ones that looked like garbage */
            InitNewGameState(); /* reset game */
            if (ComputeSaveGameChecksum(
                    (u_char *)&g_nGamestate0dMode.file->saves[0]) !=
                g_nGamestate0dMode.file->saves[0].checksum)
              BuildSaveGameBuffer(&g_nGamestate0dMode.file->saves[0]);
            if (ComputeSaveGameChecksum(
                    (u_char *)&g_nGamestate0dMode.file->saves[1]) !=
                g_nGamestate0dMode.file->saves[1].checksum)
              BuildSaveGameBuffer(&g_nGamestate0dMode.file->saves[1]);
            if (ComputeSaveGameChecksum(
                    (u_char *)&g_nGamestate0dMode.file->saves[2]) !=
                g_nGamestate0dMode.file->saves[2].checksum)
              BuildSaveGameBuffer(&g_nGamestate0dMode.file->saves[2]);

            /* go to the save file picker */
            g_nGamestate0dMode.mode = Mode_0x02;
            g_nGamestate0dMode.menuState = 4;
            g_nGamestate0dMode.selection = 1;
            g_nGamePaused = 1;
            Titlescreen_PlayUiSound(9);
          }
        } else if (g_nGamestate0dMode.mcResult == McErrFileNotExist) {
          /* no save file, ask if we can create one */
          g_nGamestate0dMode.subState = SubState_0x0A;
          g_nGamestate0dMode.selection = 0;
        } else {
          /* no memory card available anymore */
          g_nGamestate0dMode.subState = SubState_0x03;
          g_nGamestate0dMode.selection = 0;
        }
      }
      break;
    }
    case SubState_0x02: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.cardStatus[0] == McErrNone ||
          g_nGamestate0dMode.cardStatus[0] == McErrNewCard ||
          g_nGamestate0dMode.cardStatus[1] == McErrNone ||
          g_nGamestate0dMode.cardStatus[1] == McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.animTimer == 0) {
        Titlescreen_PlayUiSound(4);
      }

      if (g_nGamestate0dMode.animTimer >= 8) {
        if (ArmStoryIntroOnStart()) {
          /* triangle was pushed, ArmStoryIntroOnStart already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS)) {
          if (g_nGamestate0dMode.selection == 0) {
            g_nGamestate0dMode.subState = SubState_0x04;
            Titlescreen_PlayUiSound(3);
          } else {
            RELOAD_MEMCARDS
          }
        }
      }
      break;
    }
    case SubState_0x03: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.cardPort >= 0) {
        if (g_nGamestate0dMode.cardStatus[g_nGamestate0dMode.cardPort] != McErrNone ||
            g_nGamestate0dMode.cardStatus[1 - g_nGamestate0dMode.cardPort] ==
                McErrNewCard) {
          RELOAD_MEMCARDS
          break;
        }
      } else {
        if (g_nGamestate0dMode.cardStatus[0] == McErrNone ||
            g_nGamestate0dMode.cardStatus[0] == McErrNewCard ||
            g_nGamestate0dMode.cardStatus[1] == McErrNone ||
            g_nGamestate0dMode.cardStatus[1] == McErrNewCard) {
          RELOAD_MEMCARDS
          break;
        }
      }

      if (g_nGamestate0dMode.animTimer >= 8) {
        if (ArmStoryIntroOnStart()) {
          /* triangle was pushed, ArmStoryIntroOnStart already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS)) {
          if (g_nGamestate0dMode.selection == 0) {
            g_nGamestate0dMode.subState = SubState_0x04;
            Titlescreen_PlayUiSound(3);
          } else {
            RELOAD_MEMCARDS
          }
        }
      }
      break;
    }
    case SubState_0x04: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.cardPort >= 0) {
        if (g_nGamestate0dMode.cardStatus[g_nGamestate0dMode.cardPort] != McErrNone ||
            g_nGamestate0dMode.cardStatus[1 - g_nGamestate0dMode.cardPort] ==
                McErrNewCard) {
          RELOAD_MEMCARDS
          break;
        }
      } else {
        if (g_nGamestate0dMode.cardStatus[0] == McErrNone ||
            g_nGamestate0dMode.cardStatus[0] == McErrNewCard ||
            g_nGamestate0dMode.cardStatus[1] == McErrNone ||
            g_nGamestate0dMode.cardStatus[1] == McErrNewCard) {
          RELOAD_MEMCARDS
          break;
        }
      }

      /* mem card states still the same, process buttons after a few ticks */
      if (g_nGamestate0dMode.animTimer >= 8) {
        if (ArmStoryIntroOnStart()) {
          /* triangle was pushed, ArmStoryIntroOnStart already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          if (g_nGamestate0dMode.selection == 0) {
            InitNewGameState();
            g_nGamestate0dMode.mode = Mode_0x02;
            g_nGamestate0dMode.menuState = 5;
            g_nGamestate0dMode.demoType = 0;
            g_nSaveSlotLoaded_s.loaded = 0;
          } else {
            RELOAD_MEMCARDS
          }
        }
      }
      break;
    }
    case SubState_0x05:
    case SubState_0x06: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.cardStatus[g_nGamestate0dMode.cardPort] != McErrNone ||
          g_nGamestate0dMode.cardStatus[1 - g_nGamestate0dMode.cardPort] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.animTimer == 0) {
        Titlescreen_PlayUiSound(3);
      }

      if (g_nGamestate0dMode.animTimer >= 8) {
        if (ArmStoryIntroOnStart()) {
          /* triangle was pushed, ArmStoryIntroOnStart already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          if (g_nGamestate0dMode.selection == 0) {
            if (g_nGamestate0dMode.subState == SubState_0x05) {
              g_nGamestate0dMode.subState = SubState_0x06;
              g_nGamestate0dMode.selection = 1;
            } else {
              g_nGamestate0dMode.subState = SubState_0x07;
              MemCardSync(0, (long *)&g_nGamestate0dMode.mcCmd,
                            (long *)&g_nGamestate0dMode.mcResult);
              Titlescreen_PlayUiSound(2);
            }
          } else {
            g_nGamestate0dMode.subState = SubState_0x03;
            g_nGamestate0dMode.selection = 0;
            Titlescreen_PlayUiSound(3);
          }
        }
      }
      break;
    }
    case SubState_0x07: {
      if (g_nGamestate0dMode.animTimer >= 2) {
        g_nGamestate0dMode.mcResult =
            MemCardFormat(g_nGamestate0dMode.cardPort << 4);
        if (g_nGamestate0dMode.mcResult == McErrNone) {
          g_nGamestate0dMode.cardStatus[g_nGamestate0dMode.cardPort] = McErrNone;
          g_nGamestate0dMode.subState = SubState_0x08;
          g_nGamestate0dMode.selection = 0; /* continue */
          Titlescreen_PlayUiSound(9);
        } else {
          g_nGamestate0dMode.subState = SubState_0x09;
          g_nGamestate0dMode.selection = 1; /* cancel */
          Titlescreen_PlayUiSound(4);
        }
      }
      break;
    }
    case SubState_0x08: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.cardStatus[g_nGamestate0dMode.cardPort] != McErrNone ||
          g_nGamestate0dMode.cardStatus[1 - g_nGamestate0dMode.cardPort] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.animTimer >= 8) {
        if (ArmStoryIntroOnStart()) {
          /* triangle was pushed, ArmStoryIntroOnStart already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          Titlescreen_PlayUiSound(1);
          g_nGamestate0dMode.subState = SubState_0x01;
          g_nGamestate0dMode.menuState = 0;
          g_nGamestate0dMode.retryCount = 0;
          g_nGamestate0dMode.selection = 1;
        }
      }

      break;
    }
    case SubState_0x09: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.cardStatus[g_nGamestate0dMode.cardPort] != McErrNone ||
          g_nGamestate0dMode.cardStatus[1 - g_nGamestate0dMode.cardPort] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.animTimer >= 8) {
        if (ArmStoryIntroOnStart()) {
          /* triangle was pushed, ArmStoryIntroOnStart already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          if (g_nGamestate0dMode.selection == 0) {
            /* retry format */
            g_nGamestate0dMode.subState = SubState_0x07;
            MemCardSync(0, (long *)&g_nGamestate0dMode.mcCmd,
                          (long *)&g_nGamestate0dMode.mcResult);
            Titlescreen_PlayUiSound(2);
          } else {
            g_nGamestate0dMode.subState = SubState_0x03;
            g_nGamestate0dMode.selection = 0;
            Titlescreen_PlayUiSound(3);
          }
        }
      }
      break;
    }
    case SubState_0x0A: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.cardStatus[g_nGamestate0dMode.cardPort] != McErrNone ||
          g_nGamestate0dMode.cardStatus[1 - g_nGamestate0dMode.cardPort] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.animTimer == 0) {
        Titlescreen_PlayUiSound(3);
      }

      if (g_nGamestate0dMode.animTimer >= 8) {
        if (ArmStoryIntroOnStart()) {
          /* triangle was pushed, ArmStoryIntroOnStart already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          if (g_nGamestate0dMode.selection == 0) {
            g_nGamestate0dMode.subState = SubState_0x0B;
            g_nGamestate0dMode.menuState = 0;
            g_nGamestate0dMode.retryCount = 0;
            MemCardSync(0, (long *)&g_nGamestate0dMode.mcCmd,
                          (long *)&g_nGamestate0dMode.mcResult);
            Titlescreen_PlayUiSound(2);
          } else {
            g_nGamestate0dMode.subState = SubState_0x03;
            g_nGamestate0dMode.selection = 0;
            Titlescreen_PlayUiSound(3);
          }
        }
      }

      break;
    }
    case SubState_0x0B: {
      int i;
      if (g_nGamestate0dMode.menuState == 0) {
        g_nGamestate0dMode.menuState = 1; /* ok ... */
      } else if (g_nGamestate0dMode.menuState == 1) {
        if (g_nGamestate0dMode.retryCount == 0) {
          g_nGamestate0dMode.mcResult = MemCardCreateFile(
              g_nGamestate0dMode.cardPort << 4, (char *)s_SaveFileName, 1);
        }
        if (g_nGamestate0dMode.mcResult == McErrNone) {
          char *mcs = (char *)g_nGamestate0dMode.file;

          /* copy over the header/icons */
          CopyWords(g_nGamestate0dMode.file, g_abSaveFileHeaderTemplate,
                        sizeof(g_abSaveFileHeaderTemplate));

          g_nSaveSlotLoaded_s.fileTag = ReadRootCounter2();
          g_nGamestate0dMode.file->header.clut[15] = g_nSaveSlotLoaded_s.fileTag;

          /* reset game and create all 3 saves */
          mcs += sizeof(g_abSaveFileHeaderTemplate);
          InitNewGameState();
          for (i = 0; i < 3; ++i) {
            BuildSaveGameBuffer((SaveGameBuffer *)(mcs + i * sizeof(SaveGameBuffer)));
          }
          MemCardWrite(g_nGamestate0dMode.cardPort << 4, (char *)s_SaveFileName,
                        (unsigned long *)g_nGamestate0dMode.file, 0,
                        sizeof(Struct_0x1A00));

          g_nGamestate0dMode.menuState = 2;
        } else if (g_nGamestate0dMode.mcResult == McErrBlockFull) {
          g_nGamestate0dMode.subState = SubState_0x0C;
          g_nGamestate0dMode.selection = 0;
        } else {
          g_nGamestate0dMode.subState = SubState_0x0D;
          g_nGamestate0dMode.selection = 0;
        }
      } else if (g_nGamestate0dMode.menuState == 2) {
        /* kick back if MemCardWrite is not complete yet */
        if (MemCardSync(1, (long *)&g_nGamestate0dMode.mcCmd,
                          (long *)&g_nGamestate0dMode.mcResult) == 0)
          break;

        /* write is done, check to see if it was successful */
        if (g_nGamestate0dMode.mcResult != McErrNone) {
          g_nGamestate0dMode.subState = SubState_0x0D;
          g_nGamestate0dMode.selection = 0;
        } else {
          /* save succeeded, let's re-read it off the card */
          MemCardRead(g_nGamestate0dMode.cardPort << 4, (char *)s_SaveFileName,
                        (unsigned long *)g_nGamestate0dMode.file, 0,
                        sizeof(Struct_0x1A00));
          g_nGamestate0dMode.menuState = 3;
        }
      } else {
        /* make sure the re-read looks good */
        if (g_nGamestate0dMode.mcResult == McErrNone) {
          if (ComputeSaveGameChecksum(
                  (u_char *)&g_nGamestate0dMode.file->saves[0]) ==
                  g_nGamestate0dMode.file->saves[0].checksum &&
              ComputeSaveGameChecksum(
                  (u_char *)&g_nGamestate0dMode.file->saves[1]) ==
                  g_nGamestate0dMode.file->saves[1].checksum &&
              ComputeSaveGameChecksum(
                  (u_char *)&g_nGamestate0dMode.file->saves[2]) ==
                  g_nGamestate0dMode.file->saves[2].checksum) {
            g_nGamestate0dMode.mode = Mode_0x02;
            g_nGamestate0dMode.menuState = 4;
            g_nGamestate0dMode.selection = 0;
            g_nGamePaused = 1;
            Titlescreen_PlayUiSound(9);
          } else {
            if (g_nGamestate0dMode.retryCount < 2) {
              g_nGamestate0dMode.retryCount++;
              g_nGamestate0dMode.menuState = 2;
            } else {
              InitNewGameState();
              if (ComputeSaveGameChecksum(
                      (u_char *)&g_nGamestate0dMode.file->saves[0]) !=
                  g_nGamestate0dMode.file->saves[0].checksum)
                BuildSaveGameBuffer(&g_nGamestate0dMode.file->saves[0]);
              if (ComputeSaveGameChecksum(
                      (u_char *)&g_nGamestate0dMode.file->saves[1]) !=
                  g_nGamestate0dMode.file->saves[1].checksum)
                BuildSaveGameBuffer(&g_nGamestate0dMode.file->saves[1]);
              if (ComputeSaveGameChecksum(
                      (u_char *)&g_nGamestate0dMode.file->saves[2]) !=
                  g_nGamestate0dMode.file->saves[2].checksum)
                BuildSaveGameBuffer(&g_nGamestate0dMode.file->saves[2]);
              g_nGamestate0dMode.mode = Mode_0x02;
              g_nGamestate0dMode.menuState = 4;
              g_nGamestate0dMode.selection = 0;
              g_nGamePaused = 1;
              Titlescreen_PlayUiSound(9);
            }
          }
        } else {
          g_nGamestate0dMode.subState = SubState_0x0D;
          g_nGamestate0dMode.selection = 0;
        }
      }
      break;
    }
    case SubState_0x0C: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.cardStatus[g_nGamestate0dMode.cardPort] != McErrNone ||
          g_nGamestate0dMode.cardStatus[1 - g_nGamestate0dMode.cardPort] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.animTimer == 0) {
        Titlescreen_PlayUiSound(4);
      }

      if (g_nGamestate0dMode.animTimer >= 8) {
        if (ArmStoryIntroOnStart()) {
          /* triangle was pushed, ArmStoryIntroOnStart already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS)) {
          Titlescreen_PlayUiSound(3);
          g_nGamestate0dMode.subState = SubState_0x03;
          g_nGamestate0dMode.selection = 0;
        }
      }
      break;
    }
    case SubState_0x0D: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.cardStatus[g_nGamestate0dMode.cardPort] != McErrNone ||
          g_nGamestate0dMode.cardStatus[1 - g_nGamestate0dMode.cardPort] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.animTimer == 0) {
        Titlescreen_PlayUiSound(4);
      }

      if (g_nGamestate0dMode.animTimer >= 8) {
        if (ArmStoryIntroOnStart()) {
          /* triangle was pushed, ArmStoryIntroOnStart already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          if (g_nGamestate0dMode.selection == 0) {
            g_nGamestate0dMode.subState = SubState_0x0B;
            g_nGamestate0dMode.menuState = 0;
            g_nGamestate0dMode.retryCount = 0;
            MemCardSync(0, (long *)&g_nGamestate0dMode.mcCmd,
                          (long *)&g_nGamestate0dMode.mcResult);
            Titlescreen_PlayUiSound(2);
          } else {
            g_nGamestate0dMode.subState = SubState_0x03;
            g_nGamestate0dMode.selection = 0;
            Titlescreen_PlayUiSound(3);
          }
        }
      }
      break;
    }
    case SubState_0x0F: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.cardStatus[0] == McErrCardNotExist ||
          g_nGamestate0dMode.cardStatus[0] == McErrNewCard ||
          g_nGamestate0dMode.cardStatus[1] == McErrCardNotExist ||
          g_nGamestate0dMode.cardStatus[1] == McErrNewCard) {
        RELOAD_MEMCARDS;
        break;
      }

      if (g_nGamestate0dMode.animTimer == 0) {
        Titlescreen_PlayUiSound(3);
      }

      if (g_nGamestate0dMode.animTimer >= 8) {
        if (g_dwPadPressed.pressed & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_CROSS | PAD_START)) {
          g_nGamestate0dMode.cardPort = g_nGamestate0dMode.selection;
          if (g_nGamestate0dMode.needsFormat[g_nGamestate0dMode.selection]) {
            g_nGamestate0dMode.subState = SubState_0x05;
            g_nGamestate0dMode.selection = 0;
          } else {
            g_nGamestate0dMode.subState = SubState_0x01;
            g_nGamestate0dMode.menuState = 0;
            g_nGamestate0dMode.retryCount = 0;
            g_nGamestate0dMode.selection = 1;
            Titlescreen_PlayUiSound(1);
          }
        }
      }
      break;
    }
    }
    break;
  }
  case Mode_0x02: {
    /* while not transitioning to the demo, keep checking our card states */
    if (g_nGamestate0dMode.menuState < 5) {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.cardStatus[g_nGamestate0dMode.cardPort] != McErrNone ||
          g_nGamestate0dMode.cardStatus[1 - g_nGamestate0dMode.cardPort] ==
              McErrNewCard) {
        g_nGamestate0dMode.mode = Mode_0x01;
        g_nGamestate0dMode.subState = SubState_0x00;
        g_nGamestate0dMode.menuState = 0;
        MemCardSync(0, (long *)&g_nGamestate0dMode.mcCmd,
                      (long *)&g_nGamestate0dMode.mcResult);
        break;
      }
    }

    /* keep checking to see if we have any save files */
    g_nGamestate0dMode.apSaves[0] = &g_nGamestate0dMode.file->saves[0];
    g_nGamestate0dMode.apSaves[1] = &g_nGamestate0dMode.file->saves[1];
    g_nGamestate0dMode.apSaves[2] = &g_nGamestate0dMode.file->saves[2];
    if (!g_nGamestate0dMode.apSaves[0]->levelVisited[0] &&
        !g_nGamestate0dMode.apSaves[1]->levelVisited[0] &&
        !g_nGamestate0dMode.apSaves[2]->levelVisited[0]) {
      g_nGamestate0dMode.anySaveUsed = 0;
    } else {
      g_nGamestate0dMode.anySaveUsed = 1;
    }

    if (g_nGamestate0dMode.menuState == 0) {
      if (g_nGamestate0dMode.savedSelection == 0) {
        g_nGamestate0dMode.menuState = 1;
        if (!g_nGamestate0dMode.apSaves[0]->levelVisited[0]) {
          g_nGamestate0dMode.selection = 0;
        } else if (!g_nGamestate0dMode.apSaves[1]->levelVisited[0]) {
          g_nGamestate0dMode.selection = 1;
        } else if (!g_nGamestate0dMode.apSaves[2]->levelVisited[0]) {
          g_nGamestate0dMode.selection = 2;
        } else {
          g_nGamestate0dMode.selection = 0;
        }
      } else if (g_nGamestate0dMode.anySaveUsed) {
        g_nGamestate0dMode.menuState = 3;
        if (g_nGamestate0dMode.apSaves[0]->levelVisited[0]) {
          g_nGamestate0dMode.selection = 0;
        } else if (g_nGamestate0dMode.apSaves[1]->levelVisited[0]) {
          g_nGamestate0dMode.selection = 1;
        } else {
          g_nGamestate0dMode.selection = 2;
        }
      } else {
        g_nGamestate0dMode.selection = 1;
        g_nGamestate0dMode.menuState = 4;
      }
    } else if (g_nGamestate0dMode.menuState == 1) {
      if (g_nGamestate0dMode.animTimer >= 8) {
        if (g_dwPadPressed.pressed & (PAD_DOWN | PAD_TRIANGLE)) {
          Titlescreen_PlayUiSound(9);
          g_nGamestate0dMode.menuState = 4;
          g_nGamestate0dMode.selection = 0;
        } else if (g_dwPadPressed.pressed & PAD_LEFT) {
          g_nGamestate0dMode.animTimer = 16;
          g_nGamestate0dMode.selection -= 1;
          if (g_nGamestate0dMode.selection < 0)
            g_nGamestate0dMode.selection = 2;
          Titlescreen_PlayUiSound(0);
        } else if (g_dwPadPressed.pressed & PAD_RIGHT) {
          g_nGamestate0dMode.animTimer = 16;
          g_nGamestate0dMode.selection += 1;
          if (g_nGamestate0dMode.selection > 2)
            g_nGamestate0dMode.selection = 0;
          Titlescreen_PlayUiSound(0);
        } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS)) {
          if (g_nGamestate0dMode.apSaves[g_nGamestate0dMode.selection]
                  ->levelVisited[0]) {
            Titlescreen_PlayUiSound(3);
            g_nGamestate0dMode.menuState = 2;
            g_nGamestate0dMode.yesNoChoice = 1;
          } else {
            InitNewGameState();
            START_DEMO(0)
          }
        }
      }
    } else if (g_nGamestate0dMode.menuState == 2) {
      if (g_nGamestate0dMode.animTimer >= 8) {
        if ((g_dwPadPressed.pressed & (PAD_RIGHT | PAD_LEFT))) {
          g_nGamestate0dMode.animTimer = 16;
          g_nGamestate0dMode.yesNoChoice = 1 - g_nGamestate0dMode.yesNoChoice;
          Titlescreen_PlayUiSound(0);
        } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS | PAD_TRIANGLE)) {
          if (g_nGamestate0dMode.yesNoChoice == 1 ||
              (g_dwPadPressed.pressed & PAD_TRIANGLE) != 0) {
            g_nGamestate0dMode.menuState = 1;
            Titlescreen_PlayUiSound(4);
          } else {
            InitNewGameState();
            START_DEMO(0)
          }
        }
      }
    } else if (g_nGamestate0dMode.menuState == 3) {
      if (g_nGamestate0dMode.animTimer >= 8) {
        if (g_dwPadPressed.pressed & (PAD_DOWN | PAD_TRIANGLE)) {
          Titlescreen_PlayUiSound(9);
          g_nGamestate0dMode.menuState = 4;
          g_nGamestate0dMode.selection = 1;
        } else if (g_dwPadPressed.pressed & PAD_LEFT) {
          int i;
          for (i = g_nGamestate0dMode.selection - 1; i >= 0; --i) {
            if (g_nGamestate0dMode.apSaves[i]->levelVisited[0]) {
              g_nGamestate0dMode.animTimer = 16;
              break;
            }
          }
          if (i >= 0) {
            g_nGamestate0dMode.selection = i;
            Titlescreen_PlayUiSound(0);
          }
        } else if (g_dwPadPressed.pressed & PAD_RIGHT) {
          int i;
          for (i = g_nGamestate0dMode.selection + 1; i < 3; ++i) {
            if (g_nGamestate0dMode.apSaves[i]->levelVisited[0]) {
              g_nGamestate0dMode.animTimer = 16;
              break;
            }
          }
          if (i < 3) {
            g_nGamestate0dMode.selection = i;
            Titlescreen_PlayUiSound(0);
          }
        } else if (g_dwPadPressed.pressed & (PAD_START | PAD_CROSS)) {
          g_nSparxHealth = 3;
          g_nSparxFoodCount = 0;
          g_nSparxHealthStash = -1;
          ApplySaveGameBuffer(g_nGamestate0dMode.apSaves[g_nGamestate0dMode.selection]);
          START_DEMO(1)
        }
      }
    } else if (g_nGamestate0dMode.menuState == 4) {
      if (g_nGamestate0dMode.animTimer >= 8) {
        if (ArmStoryIntroOnStart()) {
          /* triangle was pushed, ArmStoryIntroOnStart already took care of it */
        } else if (g_dwPadPressed.pressed & (PAD_RIGHT | PAD_LEFT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.pressed & (PAD_UP | PAD_START | PAD_CROSS)) {
          if (g_nGamestate0dMode.selection == 1 && !g_nGamestate0dMode.anySaveUsed) {
            g_nGamestate0dMode.animTimer = 16;
            Titlescreen_PlayUiSound(4);
          } else {
            g_nGamestate0dMode.menuState = 0;
            g_nGamePaused = 1;
            g_nGamestate0dMode.savedSelection = g_nGamestate0dMode.selection;
            Titlescreen_PlayUiSound(1);
          }
        }
      }
    } else if (g_nGamestate0dMode.menuState == 5) {
      if (g_nGamestate0dMode.animTimer == 0) {
        Titlescreen_PlayUiSound(5);
      }

      g_nGenericCountdown = g_nGamestate0dMode.animTimer + 1;
      if (g_nGenericCountdown > 15)
        g_nGenericCountdown = 15;

      if (g_nGamestate0dMode.animTimer >= 16) {
        RECT rc;
        MemCardCloseSession(); /* MemCardStop */
        g_nGamestate0dMode.mode = Mode_0x03;
        g_nGamestate0dMode.menuState = 0;
        setRECT(&rc, 0, 0, 512, 480);
        ClearImage(&rc, 0, 0, 0);
        DrawSync(0); /* DrawSync */
      }
    }
    break;
  }
  }

  RotateLightVectorXZ(3); /* SpecularUpdate */
  if (g_dwPadPressed.padType >= 2 && (u_short)g_dwPadPressed.held != 0) {
    g_nGamestate0dMode.idleFrames = 0;
  } else {
    g_nGamestate0dMode.idleFrames++;
  }

  if (g_nGamestate0dMode.mode < 2) {
    if (g_nGamestate0dMode.subState != old_state)
      g_nGamestate0dMode.animTimer = 0;
    else
      g_nGamestate0dMode.animTimer = g_nGamestate0dMode.animTimer + 1;
  } else {
    if (g_nGamestate0dMode.menuState != old_state)
      g_nGamestate0dMode.animTimer = 0;
    else
      g_nGamestate0dMode.animTimer = g_nGamestate0dMode.animTimer + 1;
  }
}
