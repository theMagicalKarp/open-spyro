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
 * The sprite-sheet CD read pins its CD handle to $a0 and the D_8007A6D8
 * argument to $a3. As pseudos, the handle's volatile load and the a3 load are
 * both birthing-boosted, and they crowd the `- 0x40000` constant and the
 * buffer pointer into one sched1 priority group, which hands $a1 to the
 * pointer. As hard-register sets they are LUID-ordered stragglers instead:
 * the handle loads first, the a3 load last, and the constant keeps $a1.
 */

#include "titlescreen.h"

static const char s_SaveFileName[20] = "BASCUS-94228SPYRO";

#define SWAP_MENU_SELECTION                                                    \
  g_nGamestate0dMode.unk_0x0C = 16;                                                   \
  g_nGamestate0dMode.unk_0x14 = 1 - g_nGamestate0dMode.unk_0x14;               \
  Titlescreen_PlayUiSound(0);

#define RELOAD_MEMCARDS                                                        \
  g_nGamestate0dMode.unk_0x10 = SubState_0x00;                            \
  g_nGamestate0dMode.unk_0x04 = 0;                                                      \
  MemCardSync(0, (long *)&g_nGamestate0dMode.unk_0x40,                    \
                (long *)&g_nGamestate0dMode.unk_0x44);

#define START_DEMO(DEMO_TYPE)                                                  \
  g_nGamestate0dMode.unk_0x04 = 5;                                                      \
  g_nGamestate0dMode.unk_0x1C = DEMO_TYPE;                                           \
  D_80078D14.unk_0x20 = g_nGamestate0dMode.unk_0x4C->unk_0x000.unk_0x60[15];         \
  D_80078D14.unk_0x14 = 1;                                              \
  D_80078D14.unk_0x18 = g_nGamestate0dMode.unk_0x28 << 4;                \
  D_80078D14.unk_0x1c = g_nGamestate0dMode.unk_0x14;                           \
  MemCardSync(0, (long *)&g_nGamestate0dMode.unk_0x40,                    \
                (long *)&g_nGamestate0dMode.unk_0x44);

void Titlescreen_Update(void) {
  int old_state =
      g_nGamestate0dMode.unk_0x00 < 2 ? g_nGamestate0dMode.unk_0x10 : g_nGamestate0dMode.unk_0x04;

  g_pPathTableHead->unk_0x00 += 2;

  if (g_pPathTableHead->unk_0x00 < 1100) {
    g_nGamestate0dMode.unk_0x08 = 0;
    if (g_nGamestate0dMode.unk_0x10 == 2 && g_pPathTableHead->unk_0x00 >= 300 &&
        (g_dwPadPressed.unk_0x8 & (PAD_START | PAD_CROSS)) != 0) {
      StopSoundVoiceByIndex(g_bGemPickupSfxVoiceMarker, 4); /* StopSound(voice) */
      g_pPathTableHead->unk_0x00 = 1170;
    }
  } else {
    if (g_pPathTableHead->unk_0x00 == 1344 && (GetRandomU32() & 3) != 0) {
      g_pPathTableHead->unk_0x00 = 1172;
    } else if (g_pPathTableHead->unk_0x00 >= 1530) {
      g_pPathTableHead->unk_0x00 = 1172;
    }
  }

  if (g_nGenericCountdown)
    --g_nGenericCountdown;

  AdvanceGemCutsceneFrame(g_pPathTableHead->unk_0x00);

  g_nCameraEulerRoll.x = 0;
  g_nCameraEulerRoll.y += 16;

  switch (g_nGamestate0dMode.unk_0x00) {
  case Mode_0x00: {
    if (g_nGamestate0dMode.unk_0x10 < 2) {
      /* kick out while we're still loading from the CD */
      RestartStalledCdRead();
      if (g_nCdBaseLba.unk_0x28 != 0 || func_80063BD8(1, 0) != 2 ||
          (g_nCurrentMusicTrack.unk_0x004 & 0x40) == 0)
        break;

      if (g_nGamestate0dMode.unk_0x10 == 0) {
        /* kick off loading titlescreen data from the CD */
        register int cd asm("$4");
        register int t asm("$7");
        cd = *((volatile int *) &g_nCdBaseLba.unk_0x00);
        t = D_8007A6D8;
        StartCdReadAsync(cd, g_pRenderScratchRegionBase - 0x40000, 0x40000,
                      t, 600);
        g_nGamestate0dMode.unk_0x10 = 1;
      } else if (g_nGamestate0dMode.unk_0x10 == 1) {
        /* once loaded, initialize music and upload the sprite sheet to vram */
        RECT rc;
        g_nCurrentMusicTrack.unk_0x000 = 20;
        g_nCurrentMusicTrack.unk_0x21C = 155520000;
        HandleMusicCommand(20, 1);
        setRECT(&rc, 512, 256, 512, 256);
        LoadImage(&rc, g_pRenderScratchRegionBase - 0x40000);
        g_nGamestate0dMode.unk_0x10 = 2;
      }
    } else if (g_nGamestate0dMode.unk_0x10 == 2) {
      /* titlescreen sequence is playing, just waiting for a bit */
      if (g_pPathTableHead->unk_0x00 < 1169) {
        g_nGamestate0dMode.unk_0x0C = 0;
      } else {
        if (g_nGamestate0dMode.unk_0x0C == 1) {
          Titlescreen_PlayUiSound(6);
        }
        if (g_nGamestate0dMode.unk_0x0C >= 16)
          g_nGamestate0dMode.unk_0x10 = 3;
      }
    } else if (g_nGamestate0dMode.unk_0x10 == 3) {
      /* ready for button inputs or to time out to show the demo */
      if ((g_dwPadPressed.unk_0x8 & (PAD_L1 | PAD_TRIANGLE)) ==
          (PAD_L1 | PAD_TRIANGLE)) {
        StopAllSoundExceptMask(0); /* stop all sounds */
        Titlescreen_PlayUiSound(9);
        TickSpuPerFrame();
        while (g_nCdBaseLba.unk_0x28 != 0 || func_80063BD8(1, 0) != 2 ||
               (g_nCurrentMusicTrack.unk_0x004 & 0x40) == 0) {
          RestartStalledCdRead();
          TickActiveSoundVoices(); /* update sounds */
          TickCdMusicStream();
        }
        func_8002BAB8(); /* crash-demo launch */
      } else if (g_dwPadPressed.unk_0x0 & (PAD_START | PAD_CROSS)) {
        g_nGamestate0dMode.unk_0x10 = 4;
        Titlescreen_PlayUiSound(7);
      } else if (g_nGamestate0dMode.unk_0x08 >= 990) {
        g_nGamestate0dMode.unk_0x10 = 5;
      }
    } else if (g_nGamestate0dMode.unk_0x10 == 4) {
      /* animate out the logo, move to mem card screen */
      if (g_nGamestate0dMode.unk_0x0C == 16) {
        Titlescreen_PlayUiSound(8);
      }
      if (g_nGamestate0dMode.unk_0x0C >= 32) {
        g_nGamestate0dMode.unk_0x00 = Mode_0x01;
        g_nGamestate0dMode.unk_0x10 = SubState_0x00;
        g_nGamestate0dMode.unk_0x04 = 0;
        MemCardOpenSession(); /* MemCardStart */
      }
    } else if (g_nGamestate0dMode.unk_0x10 == 5) {
      /* timed out: fade out for 16 ticks and then move to demo mode */
      if (g_nGamestate0dMode.unk_0x0C == 0) {
        Titlescreen_PlayUiSound(5);
      }
      g_nGenericCountdown = g_nGamestate0dMode.unk_0x0C;
      if (g_nGenericCountdown > 15)
        g_nGenericCountdown = 15;
      if (g_nGamestate0dMode.unk_0x0C >= 16) {
        RECT rc;
        g_nGamestate0dMode.unk_0x00 = Mode_0x03;
        g_nGamestate0dMode.unk_0x04 = 0;
        g_nGamestate0dMode.unk_0x1C = 2;
        setRECT(&rc, 0, 0, 512, 480);
        ClearImage(&rc, 0, 0, 0);
        DrawSync(0); /* DrawSync */
      }
    }
    break;
  }
  case Mode_0x01: {
    switch (g_nGamestate0dMode.unk_0x10) {
    case SubState_0x00: {
      /* wait for async mem card function to finish */
      if (MemCardSync(1, (long *)&g_nGamestate0dMode.unk_0x40,
                        (long *)&g_nGamestate0dMode.unk_0x44) == 0)
        break;

      if (g_nGamestate0dMode.unk_0x04 == 0) {
        /* try to ping port A for a mem card */
        Titlescreen_PlayUiSound(2);
        MemCardLoad(0 /* port A card 0 */);
        g_nGamestate0dMode.unk_0x04 = 1;
      } else if (g_nGamestate0dMode.unk_0x04 == 1) {
        /* pull memcard state for port A */
        g_nGamestate0dMode.unk_0x30[0] = g_nGamestate0dMode.unk_0x44;

        if (g_nGamestate0dMode.unk_0x30[0] == McErrNewCard)
          g_nGamestate0dMode.unk_0x30[0] = McErrNone;
        if (g_nGamestate0dMode.unk_0x30[0] == McErrNotFormat) {
          g_nGamestate0dMode.unk_0x30[0] = McErrNone;
          g_nGamestate0dMode.unk_0x38[0] = 1;
        } else
          g_nGamestate0dMode.unk_0x38[0] = 0;

        /* now go and ping port B */
        MemCardLoad(0x10 /* port B card 0 */);
        g_nGamestate0dMode.unk_0x04 = 2;
      } else {
        /* pull memcard state for port B */
        g_nGamestate0dMode.unk_0x30[1] = g_nGamestate0dMode.unk_0x44;

        if (g_nGamestate0dMode.unk_0x30[1] == McErrNewCard)
          g_nGamestate0dMode.unk_0x30[1] = McErrNone;
        if (g_nGamestate0dMode.unk_0x30[1] == McErrNotFormat) {
          g_nGamestate0dMode.unk_0x30[1] = McErrNone;
          g_nGamestate0dMode.unk_0x38[1] = 1;
        } else {
          g_nGamestate0dMode.unk_0x38[1] = 0;
        }

        /* figure out what screen to go to for our A/B memcard states */
        if (g_nGamestate0dMode.unk_0x30[0] == McErrNone &&
            g_nGamestate0dMode.unk_0x30[1] == McErrNone) {
          /* both ports have a valid, formatted memcard, go to select menu */
          g_nGamestate0dMode.unk_0x10 = SubState_0x0F;
          g_nGamestate0dMode.unk_0x14 = 0;
        } else if (g_nGamestate0dMode.unk_0x30[0] == McErrNone &&
                   !g_nGamestate0dMode.unk_0x38[0]) {
          /* port A is valid and doesn't need formatting: select it */
          g_nGamestate0dMode.unk_0x28 = 0;
          g_nGamestate0dMode.unk_0x10 = SubState_0x01;
          g_nGamestate0dMode.unk_0x04 = 0;
          g_nGamestate0dMode.unk_0x24 = 0;
          g_nGamestate0dMode.unk_0x14 = 0;
        } else if (g_nGamestate0dMode.unk_0x30[1] == McErrNone &&
                   !g_nGamestate0dMode.unk_0x38[1]) {
          /* port B is valid and doesn't need formatting: select it */
          g_nGamestate0dMode.unk_0x28 = 1;
          g_nGamestate0dMode.unk_0x10 = SubState_0x01;
          g_nGamestate0dMode.unk_0x04 = 0;
          g_nGamestate0dMode.unk_0x24 = 0;
          g_nGamestate0dMode.unk_0x14 = 0;
        } else if (g_nGamestate0dMode.unk_0x30[0] == McErrNone &&
                   g_nGamestate0dMode.unk_0x38[0]) {
          /* port A is valid but needs a format, ask if that's ok */
          g_nGamestate0dMode.unk_0x28 = 0;
          g_nGamestate0dMode.unk_0x10 = SubState_0x05;
          g_nGamestate0dMode.unk_0x14 = 0;
        } else if (g_nGamestate0dMode.unk_0x30[1] == McErrNone &&
                   g_nGamestate0dMode.unk_0x38[1]) {
          /* port B is valid but needs a format, ask if that's ok */
          g_nGamestate0dMode.unk_0x28 = 1;
          g_nGamestate0dMode.unk_0x10 = SubState_0x05;
          g_nGamestate0dMode.unk_0x14 = 0;
        } else {
          /* no valid cards found */
          g_nGamestate0dMode.unk_0x28 = -1;
          g_nGamestate0dMode.unk_0x10 = SubState_0x02;
          g_nGamestate0dMode.unk_0x14 = 0;
        }
      }
      break;
    }
    case SubState_0x01: {
      /* wait for async mem card function to finish */
      if (MemCardSync(1, (long *)&g_nGamestate0dMode.unk_0x40,
                        (long *)&g_nGamestate0dMode.unk_0x44) == 0)
        break;

      if (g_nGamestate0dMode.unk_0x04 == 0) {
        /* read save file from the selected card */
        MemCardRead(g_nGamestate0dMode.unk_0x28 << 4, (char *)s_SaveFileName,
                      (unsigned long *)g_nGamestate0dMode.unk_0x4C, 0,
                      sizeof(Struct_0x1A00));
        g_nGamestate0dMode.unk_0x04 = 1;
      } else if (g_nGamestate0dMode.unk_0x04 == 1) {
        /* save file has been read */
        if (g_nGamestate0dMode.unk_0x44 == McErrNone) {
          /* no errors reading, check to see if data is intact */
          if (ComputeSaveGameChecksum(
                  (u_char *)&g_nGamestate0dMode.unk_0x4C->unk_0x200[0]) ==
                  g_nGamestate0dMode.unk_0x4C->unk_0x200[0].unk_0x58C &&
              ComputeSaveGameChecksum(
                  (u_char *)&g_nGamestate0dMode.unk_0x4C->unk_0x200[1]) ==
                  g_nGamestate0dMode.unk_0x4C->unk_0x200[1].unk_0x58C &&
              ComputeSaveGameChecksum(
                  (u_char *)&g_nGamestate0dMode.unk_0x4C->unk_0x200[2]) ==
                  g_nGamestate0dMode.unk_0x4C->unk_0x200[2].unk_0x58C) {
            /* save checksums matched, go to save file picker */
            g_nGamestate0dMode.unk_0x00 = Mode_0x02;
            g_nGamestate0dMode.unk_0x04 = 4;
            g_nGamestate0dMode.unk_0x14 = 1;
            g_nGamePaused = 1;
            Titlescreen_PlayUiSound(9);
          } else if (g_nGamestate0dMode.unk_0x24 < 2) {
            /* save checksums didn't match, retry the read up to 2 times */
            g_nGamestate0dMode.unk_0x24++;
            g_nGamestate0dMode.unk_0x04 = 0;
          } else {
            /* retries didn't work, recreate the ones that looked like garbage */
            InitNewGameState(); /* reset game */
            if (ComputeSaveGameChecksum(
                    (u_char *)&g_nGamestate0dMode.unk_0x4C->unk_0x200[0]) !=
                g_nGamestate0dMode.unk_0x4C->unk_0x200[0].unk_0x58C)
              BuildSaveGameBuffer(&g_nGamestate0dMode.unk_0x4C->unk_0x200[0]);
            if (ComputeSaveGameChecksum(
                    (u_char *)&g_nGamestate0dMode.unk_0x4C->unk_0x200[1]) !=
                g_nGamestate0dMode.unk_0x4C->unk_0x200[1].unk_0x58C)
              BuildSaveGameBuffer(&g_nGamestate0dMode.unk_0x4C->unk_0x200[1]);
            if (ComputeSaveGameChecksum(
                    (u_char *)&g_nGamestate0dMode.unk_0x4C->unk_0x200[2]) !=
                g_nGamestate0dMode.unk_0x4C->unk_0x200[2].unk_0x58C)
              BuildSaveGameBuffer(&g_nGamestate0dMode.unk_0x4C->unk_0x200[2]);

            /* go to the save file picker */
            g_nGamestate0dMode.unk_0x00 = Mode_0x02;
            g_nGamestate0dMode.unk_0x04 = 4;
            g_nGamestate0dMode.unk_0x14 = 1;
            g_nGamePaused = 1;
            Titlescreen_PlayUiSound(9);
          }
        } else if (g_nGamestate0dMode.unk_0x44 == McErrFileNotExist) {
          /* no save file, ask if we can create one */
          g_nGamestate0dMode.unk_0x10 = SubState_0x0A;
          g_nGamestate0dMode.unk_0x14 = 0;
        } else {
          /* no memory card available anymore */
          g_nGamestate0dMode.unk_0x10 = SubState_0x03;
          g_nGamestate0dMode.unk_0x14 = 0;
        }
      }
      break;
    }
    case SubState_0x02: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.unk_0x30[0] == McErrNone ||
          g_nGamestate0dMode.unk_0x30[0] == McErrNewCard ||
          g_nGamestate0dMode.unk_0x30[1] == McErrNone ||
          g_nGamestate0dMode.unk_0x30[1] == McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.unk_0x0C == 0) {
        Titlescreen_PlayUiSound(4);
      }

      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if (func_80032AB0()) {
          /* triangle was pushed, func_80032AB0 already took care of it */
        } else if (g_dwPadPressed.unk_0x0 & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.unk_0x0 & (PAD_START | PAD_CROSS)) {
          if (g_nGamestate0dMode.unk_0x14 == 0) {
            g_nGamestate0dMode.unk_0x10 = SubState_0x04;
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

      if (g_nGamestate0dMode.unk_0x28 >= 0) {
        if (g_nGamestate0dMode.unk_0x30[g_nGamestate0dMode.unk_0x28] != McErrNone ||
            g_nGamestate0dMode.unk_0x30[1 - g_nGamestate0dMode.unk_0x28] ==
                McErrNewCard) {
          RELOAD_MEMCARDS
          break;
        }
      } else {
        if (g_nGamestate0dMode.unk_0x30[0] == McErrNone ||
            g_nGamestate0dMode.unk_0x30[0] == McErrNewCard ||
            g_nGamestate0dMode.unk_0x30[1] == McErrNone ||
            g_nGamestate0dMode.unk_0x30[1] == McErrNewCard) {
          RELOAD_MEMCARDS
          break;
        }
      }

      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if (func_80032AB0()) {
          /* triangle was pushed, func_80032AB0 already took care of it */
        } else if (g_dwPadPressed.unk_0x0 & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.unk_0x0 & (PAD_START | PAD_CROSS)) {
          if (g_nGamestate0dMode.unk_0x14 == 0) {
            g_nGamestate0dMode.unk_0x10 = SubState_0x04;
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

      if (g_nGamestate0dMode.unk_0x28 >= 0) {
        if (g_nGamestate0dMode.unk_0x30[g_nGamestate0dMode.unk_0x28] != McErrNone ||
            g_nGamestate0dMode.unk_0x30[1 - g_nGamestate0dMode.unk_0x28] ==
                McErrNewCard) {
          RELOAD_MEMCARDS
          break;
        }
      } else {
        if (g_nGamestate0dMode.unk_0x30[0] == McErrNone ||
            g_nGamestate0dMode.unk_0x30[0] == McErrNewCard ||
            g_nGamestate0dMode.unk_0x30[1] == McErrNone ||
            g_nGamestate0dMode.unk_0x30[1] == McErrNewCard) {
          RELOAD_MEMCARDS
          break;
        }
      }

      /* mem card states still the same, process buttons after a few ticks */
      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if (func_80032AB0()) {
          /* triangle was pushed, func_80032AB0 already took care of it */
        } else if (g_dwPadPressed.unk_0x0 & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.unk_0x0 & (PAD_CROSS | PAD_START)) {
          if (g_nGamestate0dMode.unk_0x14 == 0) {
            InitNewGameState();
            g_nGamestate0dMode.unk_0x00 = Mode_0x02;
            g_nGamestate0dMode.unk_0x04 = 5;
            g_nGamestate0dMode.unk_0x1C = 0;
            D_80078D14.unk_0x14 = 0;
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

      if (g_nGamestate0dMode.unk_0x30[g_nGamestate0dMode.unk_0x28] != McErrNone ||
          g_nGamestate0dMode.unk_0x30[1 - g_nGamestate0dMode.unk_0x28] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.unk_0x0C == 0) {
        Titlescreen_PlayUiSound(3);
      }

      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if (func_80032AB0()) {
          /* triangle was pushed, func_80032AB0 already took care of it */
        } else if (g_dwPadPressed.unk_0x0 & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.unk_0x0 & (PAD_CROSS | PAD_START)) {
          if (g_nGamestate0dMode.unk_0x14 == 0) {
            if (g_nGamestate0dMode.unk_0x10 == SubState_0x05) {
              g_nGamestate0dMode.unk_0x10 = SubState_0x06;
              g_nGamestate0dMode.unk_0x14 = 1;
            } else {
              g_nGamestate0dMode.unk_0x10 = SubState_0x07;
              MemCardSync(0, (long *)&g_nGamestate0dMode.unk_0x40,
                            (long *)&g_nGamestate0dMode.unk_0x44);
              Titlescreen_PlayUiSound(2);
            }
          } else {
            g_nGamestate0dMode.unk_0x10 = SubState_0x03;
            g_nGamestate0dMode.unk_0x14 = 0;
            Titlescreen_PlayUiSound(3);
          }
        }
      }
      break;
    }
    case SubState_0x07: {
      if (g_nGamestate0dMode.unk_0x0C >= 2) {
        g_nGamestate0dMode.unk_0x44 =
            func_80067B28(g_nGamestate0dMode.unk_0x28 << 4);
        if (g_nGamestate0dMode.unk_0x44 == McErrNone) {
          g_nGamestate0dMode.unk_0x30[g_nGamestate0dMode.unk_0x28] = McErrNone;
          g_nGamestate0dMode.unk_0x10 = SubState_0x08;
          g_nGamestate0dMode.unk_0x14 = 0; /* continue */
          Titlescreen_PlayUiSound(9);
        } else {
          g_nGamestate0dMode.unk_0x10 = SubState_0x09;
          g_nGamestate0dMode.unk_0x14 = 1; /* cancel */
          Titlescreen_PlayUiSound(4);
        }
      }
      break;
    }
    case SubState_0x08: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.unk_0x30[g_nGamestate0dMode.unk_0x28] != McErrNone ||
          g_nGamestate0dMode.unk_0x30[1 - g_nGamestate0dMode.unk_0x28] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if (func_80032AB0()) {
          /* triangle was pushed, func_80032AB0 already took care of it */
        } else if (g_dwPadPressed.unk_0x0 & (PAD_CROSS | PAD_START)) {
          Titlescreen_PlayUiSound(1);
          g_nGamestate0dMode.unk_0x10 = SubState_0x01;
          g_nGamestate0dMode.unk_0x04 = 0;
          g_nGamestate0dMode.unk_0x24 = 0;
          g_nGamestate0dMode.unk_0x14 = 1;
        }
      }

      break;
    }
    case SubState_0x09: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.unk_0x30[g_nGamestate0dMode.unk_0x28] != McErrNone ||
          g_nGamestate0dMode.unk_0x30[1 - g_nGamestate0dMode.unk_0x28] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if (func_80032AB0()) {
          /* triangle was pushed, func_80032AB0 already took care of it */
        } else if (g_dwPadPressed.unk_0x0 & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.unk_0x0 & (PAD_CROSS | PAD_START)) {
          if (g_nGamestate0dMode.unk_0x14 == 0) {
            /* retry format */
            g_nGamestate0dMode.unk_0x10 = SubState_0x07;
            MemCardSync(0, (long *)&g_nGamestate0dMode.unk_0x40,
                          (long *)&g_nGamestate0dMode.unk_0x44);
            Titlescreen_PlayUiSound(2);
          } else {
            g_nGamestate0dMode.unk_0x10 = SubState_0x03;
            g_nGamestate0dMode.unk_0x14 = 0;
            Titlescreen_PlayUiSound(3);
          }
        }
      }
      break;
    }
    case SubState_0x0A: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.unk_0x30[g_nGamestate0dMode.unk_0x28] != McErrNone ||
          g_nGamestate0dMode.unk_0x30[1 - g_nGamestate0dMode.unk_0x28] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.unk_0x0C == 0) {
        Titlescreen_PlayUiSound(3);
      }

      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if (func_80032AB0()) {
          /* triangle was pushed, func_80032AB0 already took care of it */
        } else if (g_dwPadPressed.unk_0x0 & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.unk_0x0 & (PAD_CROSS | PAD_START)) {
          if (g_nGamestate0dMode.unk_0x14 == 0) {
            g_nGamestate0dMode.unk_0x10 = SubState_0x0B;
            g_nGamestate0dMode.unk_0x04 = 0;
            g_nGamestate0dMode.unk_0x24 = 0;
            MemCardSync(0, (long *)&g_nGamestate0dMode.unk_0x40,
                          (long *)&g_nGamestate0dMode.unk_0x44);
            Titlescreen_PlayUiSound(2);
          } else {
            g_nGamestate0dMode.unk_0x10 = SubState_0x03;
            g_nGamestate0dMode.unk_0x14 = 0;
            Titlescreen_PlayUiSound(3);
          }
        }
      }

      break;
    }
    case SubState_0x0B: {
      int i;
      if (g_nGamestate0dMode.unk_0x04 == 0) {
        g_nGamestate0dMode.unk_0x04 = 1; /* ok ... */
      } else if (g_nGamestate0dMode.unk_0x04 == 1) {
        if (g_nGamestate0dMode.unk_0x24 == 0) {
          g_nGamestate0dMode.unk_0x44 = func_80067718(
              g_nGamestate0dMode.unk_0x28 << 4, (char *)s_SaveFileName, 1);
        }
        if (g_nGamestate0dMode.unk_0x44 == McErrNone) {
          char *mcs = (char *)g_nGamestate0dMode.unk_0x4C;

          /* copy over the header/icons */
          CopyWords(g_nGamestate0dMode.unk_0x4C, D_8006F494,
                        sizeof(D_8006F494));

          D_80078D14.unk_0x20 = func_80017FD4();
          g_nGamestate0dMode.unk_0x4C->unk_0x000.unk_0x60[15] = D_80078D14.unk_0x20;

          /* reset game and create all 3 saves */
          mcs += sizeof(D_8006F494);
          InitNewGameState();
          for (i = 0; i < 3; ++i) {
            BuildSaveGameBuffer((Struct_0x600 *)(mcs + i * sizeof(Struct_0x600)));
          }
          MemCardWrite(g_nGamestate0dMode.unk_0x28 << 4, (char *)s_SaveFileName,
                        (unsigned long *)g_nGamestate0dMode.unk_0x4C, 0,
                        sizeof(Struct_0x1A00));

          g_nGamestate0dMode.unk_0x04 = 2;
        } else if (g_nGamestate0dMode.unk_0x44 == McErrBlockFull) {
          g_nGamestate0dMode.unk_0x10 = SubState_0x0C;
          g_nGamestate0dMode.unk_0x14 = 0;
        } else {
          g_nGamestate0dMode.unk_0x10 = SubState_0x0D;
          g_nGamestate0dMode.unk_0x14 = 0;
        }
      } else if (g_nGamestate0dMode.unk_0x04 == 2) {
        /* kick back if MemCardWrite is not complete yet */
        if (MemCardSync(1, (long *)&g_nGamestate0dMode.unk_0x40,
                          (long *)&g_nGamestate0dMode.unk_0x44) == 0)
          break;

        /* write is done, check to see if it was successful */
        if (g_nGamestate0dMode.unk_0x44 != McErrNone) {
          g_nGamestate0dMode.unk_0x10 = SubState_0x0D;
          g_nGamestate0dMode.unk_0x14 = 0;
        } else {
          /* save succeeded, let's re-read it off the card */
          MemCardRead(g_nGamestate0dMode.unk_0x28 << 4, (char *)s_SaveFileName,
                        (unsigned long *)g_nGamestate0dMode.unk_0x4C, 0,
                        sizeof(Struct_0x1A00));
          g_nGamestate0dMode.unk_0x04 = 3;
        }
      } else {
        /* make sure the re-read looks good */
        if (g_nGamestate0dMode.unk_0x44 == McErrNone) {
          if (ComputeSaveGameChecksum(
                  (u_char *)&g_nGamestate0dMode.unk_0x4C->unk_0x200[0]) ==
                  g_nGamestate0dMode.unk_0x4C->unk_0x200[0].unk_0x58C &&
              ComputeSaveGameChecksum(
                  (u_char *)&g_nGamestate0dMode.unk_0x4C->unk_0x200[1]) ==
                  g_nGamestate0dMode.unk_0x4C->unk_0x200[1].unk_0x58C &&
              ComputeSaveGameChecksum(
                  (u_char *)&g_nGamestate0dMode.unk_0x4C->unk_0x200[2]) ==
                  g_nGamestate0dMode.unk_0x4C->unk_0x200[2].unk_0x58C) {
            g_nGamestate0dMode.unk_0x00 = Mode_0x02;
            g_nGamestate0dMode.unk_0x04 = 4;
            g_nGamestate0dMode.unk_0x14 = 0;
            g_nGamePaused = 1;
            Titlescreen_PlayUiSound(9);
          } else {
            if (g_nGamestate0dMode.unk_0x24 < 2) {
              g_nGamestate0dMode.unk_0x24++;
              g_nGamestate0dMode.unk_0x04 = 2;
            } else {
              InitNewGameState();
              if (ComputeSaveGameChecksum(
                      (u_char *)&g_nGamestate0dMode.unk_0x4C->unk_0x200[0]) !=
                  g_nGamestate0dMode.unk_0x4C->unk_0x200[0].unk_0x58C)
                BuildSaveGameBuffer(&g_nGamestate0dMode.unk_0x4C->unk_0x200[0]);
              if (ComputeSaveGameChecksum(
                      (u_char *)&g_nGamestate0dMode.unk_0x4C->unk_0x200[1]) !=
                  g_nGamestate0dMode.unk_0x4C->unk_0x200[1].unk_0x58C)
                BuildSaveGameBuffer(&g_nGamestate0dMode.unk_0x4C->unk_0x200[1]);
              if (ComputeSaveGameChecksum(
                      (u_char *)&g_nGamestate0dMode.unk_0x4C->unk_0x200[2]) !=
                  g_nGamestate0dMode.unk_0x4C->unk_0x200[2].unk_0x58C)
                BuildSaveGameBuffer(&g_nGamestate0dMode.unk_0x4C->unk_0x200[2]);
              g_nGamestate0dMode.unk_0x00 = Mode_0x02;
              g_nGamestate0dMode.unk_0x04 = 4;
              g_nGamestate0dMode.unk_0x14 = 0;
              g_nGamePaused = 1;
              Titlescreen_PlayUiSound(9);
            }
          }
        } else {
          g_nGamestate0dMode.unk_0x10 = SubState_0x0D;
          g_nGamestate0dMode.unk_0x14 = 0;
        }
      }
      break;
    }
    case SubState_0x0C: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.unk_0x30[g_nGamestate0dMode.unk_0x28] != McErrNone ||
          g_nGamestate0dMode.unk_0x30[1 - g_nGamestate0dMode.unk_0x28] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.unk_0x0C == 0) {
        Titlescreen_PlayUiSound(4);
      }

      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if (func_80032AB0()) {
          /* triangle was pushed, func_80032AB0 already took care of it */
        } else if (g_dwPadPressed.unk_0x0 & (PAD_START | PAD_CROSS)) {
          Titlescreen_PlayUiSound(3);
          g_nGamestate0dMode.unk_0x10 = SubState_0x03;
          g_nGamestate0dMode.unk_0x14 = 0;
        }
      }
      break;
    }
    case SubState_0x0D: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.unk_0x30[g_nGamestate0dMode.unk_0x28] != McErrNone ||
          g_nGamestate0dMode.unk_0x30[1 - g_nGamestate0dMode.unk_0x28] ==
              McErrNewCard) {
        RELOAD_MEMCARDS
        break;
      }

      if (g_nGamestate0dMode.unk_0x0C == 0) {
        Titlescreen_PlayUiSound(4);
      }

      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if (func_80032AB0()) {
          /* triangle was pushed, func_80032AB0 already took care of it */
        } else if (g_dwPadPressed.unk_0x0 & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.unk_0x0 & (PAD_CROSS | PAD_START)) {
          if (g_nGamestate0dMode.unk_0x14 == 0) {
            g_nGamestate0dMode.unk_0x10 = SubState_0x0B;
            g_nGamestate0dMode.unk_0x04 = 0;
            g_nGamestate0dMode.unk_0x24 = 0;
            MemCardSync(0, (long *)&g_nGamestate0dMode.unk_0x40,
                          (long *)&g_nGamestate0dMode.unk_0x44);
            Titlescreen_PlayUiSound(2);
          } else {
            g_nGamestate0dMode.unk_0x10 = SubState_0x03;
            g_nGamestate0dMode.unk_0x14 = 0;
            Titlescreen_PlayUiSound(3);
          }
        }
      }
      break;
    }
    case SubState_0x0F: {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.unk_0x30[0] == McErrCardNotExist ||
          g_nGamestate0dMode.unk_0x30[0] == McErrNewCard ||
          g_nGamestate0dMode.unk_0x30[1] == McErrCardNotExist ||
          g_nGamestate0dMode.unk_0x30[1] == McErrNewCard) {
        RELOAD_MEMCARDS;
        break;
      }

      if (g_nGamestate0dMode.unk_0x0C == 0) {
        Titlescreen_PlayUiSound(3);
      }

      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if (g_dwPadPressed.unk_0x0 & (PAD_LEFT | PAD_RIGHT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.unk_0x0 & (PAD_CROSS | PAD_START)) {
          g_nGamestate0dMode.unk_0x28 = g_nGamestate0dMode.unk_0x14;
          if (g_nGamestate0dMode.unk_0x38[g_nGamestate0dMode.unk_0x14]) {
            g_nGamestate0dMode.unk_0x10 = SubState_0x05;
            g_nGamestate0dMode.unk_0x14 = 0;
          } else {
            g_nGamestate0dMode.unk_0x10 = SubState_0x01;
            g_nGamestate0dMode.unk_0x04 = 0;
            g_nGamestate0dMode.unk_0x24 = 0;
            g_nGamestate0dMode.unk_0x14 = 1;
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
    if (g_nGamestate0dMode.unk_0x04 < 5) {
      PollMemCardSyncRing();

      if (g_nGamestate0dMode.unk_0x30[g_nGamestate0dMode.unk_0x28] != McErrNone ||
          g_nGamestate0dMode.unk_0x30[1 - g_nGamestate0dMode.unk_0x28] ==
              McErrNewCard) {
        g_nGamestate0dMode.unk_0x00 = Mode_0x01;
        g_nGamestate0dMode.unk_0x10 = SubState_0x00;
        g_nGamestate0dMode.unk_0x04 = 0;
        MemCardSync(0, (long *)&g_nGamestate0dMode.unk_0x40,
                      (long *)&g_nGamestate0dMode.unk_0x44);
        break;
      }
    }

    /* keep checking to see if we have any save files */
    g_nGamestate0dMode.unk_0x50[0] = &g_nGamestate0dMode.unk_0x4C->unk_0x200[0];
    g_nGamestate0dMode.unk_0x50[1] = &g_nGamestate0dMode.unk_0x4C->unk_0x200[1];
    g_nGamestate0dMode.unk_0x50[2] = &g_nGamestate0dMode.unk_0x4C->unk_0x200[2];
    if (!g_nGamestate0dMode.unk_0x50[0]->unk_0x40[0] &&
        !g_nGamestate0dMode.unk_0x50[1]->unk_0x40[0] &&
        !g_nGamestate0dMode.unk_0x50[2]->unk_0x40[0]) {
      g_nGamestate0dMode.unk_0x48 = 0;
    } else {
      g_nGamestate0dMode.unk_0x48 = 1;
    }

    if (g_nGamestate0dMode.unk_0x04 == 0) {
      if (g_nGamestate0dMode.unk_0x20 == 0) {
        g_nGamestate0dMode.unk_0x04 = 1;
        if (!g_nGamestate0dMode.unk_0x50[0]->unk_0x40[0]) {
          g_nGamestate0dMode.unk_0x14 = 0;
        } else if (!g_nGamestate0dMode.unk_0x50[1]->unk_0x40[0]) {
          g_nGamestate0dMode.unk_0x14 = 1;
        } else if (!g_nGamestate0dMode.unk_0x50[2]->unk_0x40[0]) {
          g_nGamestate0dMode.unk_0x14 = 2;
        } else {
          g_nGamestate0dMode.unk_0x14 = 0;
        }
      } else if (g_nGamestate0dMode.unk_0x48) {
        g_nGamestate0dMode.unk_0x04 = 3;
        if (g_nGamestate0dMode.unk_0x50[0]->unk_0x40[0]) {
          g_nGamestate0dMode.unk_0x14 = 0;
        } else if (g_nGamestate0dMode.unk_0x50[1]->unk_0x40[0]) {
          g_nGamestate0dMode.unk_0x14 = 1;
        } else {
          g_nGamestate0dMode.unk_0x14 = 2;
        }
      } else {
        g_nGamestate0dMode.unk_0x14 = 1;
        g_nGamestate0dMode.unk_0x04 = 4;
      }
    } else if (g_nGamestate0dMode.unk_0x04 == 1) {
      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if (g_dwPadPressed.unk_0x0 & (PAD_DOWN | PAD_TRIANGLE)) {
          Titlescreen_PlayUiSound(9);
          g_nGamestate0dMode.unk_0x04 = 4;
          g_nGamestate0dMode.unk_0x14 = 0;
        } else if (g_dwPadPressed.unk_0x0 & PAD_LEFT) {
          g_nGamestate0dMode.unk_0x0C = 16;
          g_nGamestate0dMode.unk_0x14 -= 1;
          if (g_nGamestate0dMode.unk_0x14 < 0)
            g_nGamestate0dMode.unk_0x14 = 2;
          Titlescreen_PlayUiSound(0);
        } else if (g_dwPadPressed.unk_0x0 & PAD_RIGHT) {
          g_nGamestate0dMode.unk_0x0C = 16;
          g_nGamestate0dMode.unk_0x14 += 1;
          if (g_nGamestate0dMode.unk_0x14 > 2)
            g_nGamestate0dMode.unk_0x14 = 0;
          Titlescreen_PlayUiSound(0);
        } else if (g_dwPadPressed.unk_0x0 & (PAD_START | PAD_CROSS)) {
          if (g_nGamestate0dMode.unk_0x50[g_nGamestate0dMode.unk_0x14]
                  ->unk_0x40[0]) {
            Titlescreen_PlayUiSound(3);
            g_nGamestate0dMode.unk_0x04 = 2;
            g_nGamestate0dMode.unk_0x18 = 1;
          } else {
            InitNewGameState();
            START_DEMO(0)
          }
        }
      }
    } else if (g_nGamestate0dMode.unk_0x04 == 2) {
      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if ((g_dwPadPressed.unk_0x0 & (PAD_RIGHT | PAD_LEFT))) {
          g_nGamestate0dMode.unk_0x0C = 16;
          g_nGamestate0dMode.unk_0x18 = 1 - g_nGamestate0dMode.unk_0x18;
          Titlescreen_PlayUiSound(0);
        } else if (g_dwPadPressed.unk_0x0 & (PAD_START | PAD_CROSS | PAD_TRIANGLE)) {
          if (g_nGamestate0dMode.unk_0x18 == 1 ||
              (g_dwPadPressed.unk_0x0 & PAD_TRIANGLE) != 0) {
            g_nGamestate0dMode.unk_0x04 = 1;
            Titlescreen_PlayUiSound(4);
          } else {
            InitNewGameState();
            START_DEMO(0)
          }
        }
      }
    } else if (g_nGamestate0dMode.unk_0x04 == 3) {
      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if (g_dwPadPressed.unk_0x0 & (PAD_DOWN | PAD_TRIANGLE)) {
          Titlescreen_PlayUiSound(9);
          g_nGamestate0dMode.unk_0x04 = 4;
          g_nGamestate0dMode.unk_0x14 = 1;
        } else if (g_dwPadPressed.unk_0x0 & PAD_LEFT) {
          int i;
          for (i = g_nGamestate0dMode.unk_0x14 - 1; i >= 0; --i) {
            if (g_nGamestate0dMode.unk_0x50[i]->unk_0x40[0]) {
              g_nGamestate0dMode.unk_0x0C = 16;
              break;
            }
          }
          if (i >= 0) {
            g_nGamestate0dMode.unk_0x14 = i;
            Titlescreen_PlayUiSound(0);
          }
        } else if (g_dwPadPressed.unk_0x0 & PAD_RIGHT) {
          int i;
          for (i = g_nGamestate0dMode.unk_0x14 + 1; i < 3; ++i) {
            if (g_nGamestate0dMode.unk_0x50[i]->unk_0x40[0]) {
              g_nGamestate0dMode.unk_0x0C = 16;
              break;
            }
          }
          if (i < 3) {
            g_nGamestate0dMode.unk_0x14 = i;
            Titlescreen_PlayUiSound(0);
          }
        } else if (g_dwPadPressed.unk_0x0 & (PAD_START | PAD_CROSS)) {
          g_nLevelReadyFlag = 3;
          g_nSparxFoodCount = 0;
          g_nLevelReadyFlagStash = -1;
          func_80059594(g_nGamestate0dMode.unk_0x50[g_nGamestate0dMode.unk_0x14]);
          START_DEMO(1)
        }
      }
    } else if (g_nGamestate0dMode.unk_0x04 == 4) {
      if (g_nGamestate0dMode.unk_0x0C >= 8) {
        if (func_80032AB0()) {
          /* triangle was pushed, func_80032AB0 already took care of it */
        } else if (g_dwPadPressed.unk_0x0 & (PAD_RIGHT | PAD_LEFT)) {
          SWAP_MENU_SELECTION
        } else if (g_dwPadPressed.unk_0x0 & (PAD_UP | PAD_START | PAD_CROSS)) {
          if (g_nGamestate0dMode.unk_0x14 == 1 && !g_nGamestate0dMode.unk_0x48) {
            g_nGamestate0dMode.unk_0x0C = 16;
            Titlescreen_PlayUiSound(4);
          } else {
            g_nGamestate0dMode.unk_0x04 = 0;
            g_nGamePaused = 1;
            g_nGamestate0dMode.unk_0x20 = g_nGamestate0dMode.unk_0x14;
            Titlescreen_PlayUiSound(1);
          }
        }
      }
    } else if (g_nGamestate0dMode.unk_0x04 == 5) {
      if (g_nGamestate0dMode.unk_0x0C == 0) {
        Titlescreen_PlayUiSound(5);
      }

      g_nGenericCountdown = g_nGamestate0dMode.unk_0x0C + 1;
      if (g_nGenericCountdown > 15)
        g_nGenericCountdown = 15;

      if (g_nGamestate0dMode.unk_0x0C >= 16) {
        RECT rc;
        MemCardCloseSession(); /* MemCardStop */
        g_nGamestate0dMode.unk_0x00 = Mode_0x03;
        g_nGamestate0dMode.unk_0x04 = 0;
        setRECT(&rc, 0, 0, 512, 480);
        ClearImage(&rc, 0, 0, 0);
        DrawSync(0); /* DrawSync */
      }
    }
    break;
  }
  }

  RotateLightVectorXZ(3); /* SpecularUpdate */
  if (g_dwPadPressed.unk_0xC >= 2 && (u_short)g_dwPadPressed.unk_0x8 != 0) {
    g_nGamestate0dMode.unk_0x08 = 0;
  } else {
    g_nGamestate0dMode.unk_0x08++;
  }

  if (g_nGamestate0dMode.unk_0x00 < 2) {
    if (g_nGamestate0dMode.unk_0x10 != old_state)
      g_nGamestate0dMode.unk_0x0C = 0;
    else
      g_nGamestate0dMode.unk_0x0C = g_nGamestate0dMode.unk_0x0C + 1;
  } else {
    if (g_nGamestate0dMode.unk_0x04 != old_state)
      g_nGamestate0dMode.unk_0x0C = 0;
    else
      g_nGamestate0dMode.unk_0x0C = g_nGamestate0dMode.unk_0x0C + 1;
  }
}
