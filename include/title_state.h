/* title_state.h — the title screen's state block (0x80078d78, TitleState in
 * the titlescreen overlay). Main reads its first word as g_nTitleMode and two
 * later words as g_nStoryIntroSubstate (+0x04) / g_nTitleExitPath (+0x1C). */

#ifndef OPEN_SPYRO_TITLE_STATE_H
#define OPEN_SPYRO_TITLE_STATE_H

/* g_nTitleMode: TitleState.mode. Modes 0..2 run Titlescreen_Update/_Draw in
 * the overlay; mode 3 hands GS_TITLE to StoryIntro_Update/_Draw in main. */
enum TitleMode {
  TITLE_MODE_ATTRACT = 0,      /* logo fly-in, "press start", idle -> demo */
  TITLE_MODE_CARD_CHECK = 1,   /* memory-card probe / format / create dialogs */
  TITLE_MODE_SAVE_SELECT = 2,  /* new game / load game and the slot pickers */
  TITLE_MODE_STORY_INTRO = 3,  /* leave the title: story intro or level load */
};

/* TitleState.subState while mode == TITLE_MODE_ATTRACT. */
enum TitleAttractStep {
  TITLE_ATTRACT_LOAD = 0,        /* kick the title-scene CD read */
  TITLE_ATTRACT_LOAD_WAIT = 1,   /* start music, upload the sprite sheet */
  TITLE_ATTRACT_FLY_IN = 2,      /* scripted flight; logo slides in */
  TITLE_ATTRACT_PRESS_START = 3, /* logo + "press start"; times out to the demo */
  TITLE_ATTRACT_LOGO_OUT = 4,    /* Start pressed: logo slides out, border in */
  TITLE_ATTRACT_IDLE_FADE = 5,   /* idle timeout: fade, then exit to the demo */
};

/* TitleState.subState while mode == TITLE_MODE_CARD_CHECK. Every dialog
 * re-probes (RELOAD_MEMCARDS) when the cards change under it. */
enum TitleCardStep {
  TITLE_CARD_PROBE = 0,          /* MemCardLoad port A then B, pick a dialog */
  TITLE_CARD_READ = 1,           /* read + checksum the save file (2 retries) */
  TITLE_CARD_NONE = 2,           /* no card: continue without saving / retry */
  TITLE_CARD_UNUSABLE = 3,       /* card lost or a dialog cancelled: same choice */
  TITLE_CARD_NO_SAVE_WARNING = 4,/* continue without saving? -> new game */
  TITLE_CARD_FORMAT_ASK = 5,     /* card unformatted: format / cancel */
  TITLE_CARD_FORMAT_CONFIRM = 6, /* second format confirmation */
  TITLE_CARD_FORMATTING = 7,     /* MemCardFormat */
  TITLE_CARD_FORMAT_DONE = 8,    /* formatted: press to continue -> READ */
  TITLE_CARD_FORMAT_FAILED = 9,  /* retry / cancel */
  TITLE_CARD_CREATE_ASK = 10,    /* no save file: create / cancel */
  TITLE_CARD_CREATING = 11,      /* create + write three fresh saves, re-read */
  TITLE_CARD_FULL = 12,          /* McErrBlockFull: press to continue */
  TITLE_CARD_CREATE_FAILED = 13, /* retry / cancel */
  TITLE_CARD_PICK_SLOT = 15,     /* cards in both slots: choose one */
};

/* TitleState.menuState while mode == TITLE_MODE_SAVE_SELECT. */
enum TitleMenu {
  TITLE_MENU_ROUTE = 0,          /* pick the next screen from savedSelection */
  TITLE_MENU_NEW_SLOT = 1,       /* choose a slot for a new game */
  TITLE_MENU_OVERWRITE = 2,      /* confirm overwriting a used slot */
  TITLE_MENU_LOAD_SLOT = 3,      /* choose a used slot to load */
  TITLE_MENU_NEW_OR_LOAD = 4,    /* top menu: new game / load game */
  TITLE_MENU_FADE_OUT = 5,       /* fade, close the card session, exit */
};

/* g_nTitleExitPath (TitleState +0x1C): where StoryIntro_Update goes. */
enum TitleExitPath {
  TITLE_EXIT_NEW_GAME = 0,       /* opening-story cutscene */
  TITLE_EXIT_LOADED_GAME = 1,    /* the loaded save's level (also the
                                  * special-cutscene return path) */
  TITLE_EXIT_DEMO = 2,           /* attract demo level */
};

#endif
