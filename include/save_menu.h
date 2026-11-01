/* save_menu.h — GS_SAVE_MENU, the prompt a save host (type 110) opens
 * (BeginSaveMenuFromWorld, SaveMenu_Update, SaveLoadMenu_Draw). The record at
 * 0x80078d00 holds mode [0], phase [1], cursor [2] and substate [3]. */

#ifndef OPEN_SPYRO_SAVE_MENU_H
#define OPEN_SPYRO_SAVE_MENU_H

#include "memcard.h"

/* g_nSaveMenuMode */
enum SaveMenuMode {
  SAVE_MODE_SWING_IN = 0, /* host, Sparx and camera ease to their keyframes */
  SAVE_MODE_MENU = 1,
};

/* g_nSaveMenuSubstate: the screen shown in SAVE_MODE_MENU. */
enum SaveMenuScreen {
  SAVE_SCREEN_MAIN = 0,         /* HI SPYRO: save game / replay dragon / continue */
  SAVE_SCREEN_NO_SESSION = 1,   /* game not started from a card: restart with one */
  SAVE_SCREEN_SAVING = 2,       /* the card steps below, in the cursor word */
  SAVE_SCREEN_NO_CARD = 3,      /* NO MEMORY CARD: insert the card with this save */
  SAVE_SCREEN_NO_FILE = 4,      /* NO SAVE FILE (unformatted card or other file) */
  SAVE_SCREEN_ERROR = 5,        /* SAVE ERROR: is the card still in place */
  SAVE_SCREEN_FAILED = 6,       /* SAVE FAILED: retry / abort */
  SAVE_SCREEN_SAVED = 7,        /* GAME SAVED */
};

/* g_nSaveMenuCursor on SAVE_SCREEN_MAIN */
enum SaveMenuRow {
  SAVE_ROW_SAVE = 0,
  SAVE_ROW_REPLAY_DRAGON = 1, /* replay the rescue of the host's dragon */
  SAVE_ROW_CONTINUE = 2,
  SAVE_ROW_COUNT = 3,
};

/* g_nSaveMenuCursor on SAVE_SCREEN_FAILED */
enum SaveFailedRow {
  SAVE_FAILED_RETRY = 0,
  SAVE_FAILED_ABORT = 1,
};

/* g_nSaveMenuCursor during SAVE_SCREEN_SAVING */
enum SaveStep {
  SAVE_STEP_OPEN = 0,       /* open the session, MemCardLoad the slot */
  SAVE_STEP_READ_HEADER = 1,/* read the file header */
  SAVE_STEP_WRITE = 2,      /* file tag matches: build and write the save */
  SAVE_STEP_READ_BACK = 3,  /* read the written save back */
  SAVE_STEP_VERIFY = 4,     /* checksum it */
};

#endif /* OPEN_SPYRO_SAVE_MENU_H */
