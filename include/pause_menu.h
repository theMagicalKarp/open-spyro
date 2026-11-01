/* pause_menu.h — GS_PAUSE_MENU pages and rows (PauseMenu_Update, drawn by
 * Gamestate02_03_06_Draw). g_nPauseMenuSubstate is the page,
 * g_nPauseMenuCursor the row on it. */

#ifndef OPEN_SPYRO_PAUSE_MENU_H
#define OPEN_SPYRO_PAUSE_MENU_H

enum PausePage {
  PAUSE_PAGE_MAIN = 0,
  PAUSE_PAGE_OPTIONS = 1,
  PAUSE_PAGE_QUIT_CONFIRM = 2, /* home worlds only: quit to the title */
};

enum PauseMainRow {
  PAUSE_ROW_CONTINUE = 0,
  PAUSE_ROW_OPTIONS = 1,
  PAUSE_ROW_INVENTORY = 2,
  PAUSE_ROW_EXIT = 3, /* QUIT (flight level), EXIT LEVEL, or QUIT GAME (home world) */
};

enum PauseOptionsRow {
  OPTIONS_ROW_SFX_VOLUME = 0,
  OPTIONS_ROW_MUSIC_VOLUME = 1,
  OPTIONS_ROW_SPEAKERS = 2,  /* stereo / mono */
  OPTIONS_ROW_VIBRATION = 3, /* skipped unless a DualShock is attached */
  OPTIONS_ROW_CAMERA = 4,    /* active / passive (CAM_OPT_*) */
  OPTIONS_ROW_DONE = 5,
  OPTIONS_ROW_COUNT = 6,
};

enum PauseQuitRow {
  PAUSE_QUIT_YES = 0,
  PAUSE_QUIT_NO = 1,
};

#endif /* OPEN_SPYRO_PAUSE_MENU_H */
