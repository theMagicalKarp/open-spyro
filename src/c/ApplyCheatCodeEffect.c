#include "actor_cutscene.h"
#include "funcs.h"
#include "globals.h"
#include "level_sfx.h"
#include "sample_bank.h"

/* g_anCheatCodeSequences slots that do something (CheckCheatCodeMatch passes
   the matched slot). Slots 0, 1 and 5..7 have sequences too and still match,
   but have no case here. */
enum CheatCode {
  CHEAT_CREDITS = 2,    /* roll the credits, then return to this level */
  CHEAT_ALL_WORLDS = 3, /* every level visited, every balloonist open */
  CHEAT_99_LIVES = 4,
};

/* Apply a matched cheat code (0x8002d810): CHEAT_CREDITS jumps to the credits
   (remembering the level to return to); CHEAT_ALL_WORLDS marks levels 0..0x1F
   visited and every world's balloonist BALLOONIST_OPEN; both play the reward
   sound. CHEAT_99_LIVES sets 99 lives. */

void ApplyCheatCodeEffect(int code) {
  unsigned char val;
  int n;
  unsigned char *p;
  unsigned char *q;
  switch (code) {
  case CHEAT_CREDITS:
    g_nCreditsReturnLevelId = g_nActiveLevelId;
    BeginCreditsSequence(0);
    PlaySoundEffect(LEVEL_SFX(SFX_REWARD), 0, SFX_MODE_PANNED, 0);
    break;
  case CHEAT_ALL_WORLDS:
    val = 1;
    n = 0x1F;
    p = &g_abLevelVisitedFlag[0x1F];
    do {
      *p = val;
      n -= 1;
      p -= 1;
    } while (n >= 0);
    val = BALLOONIST_OPEN;
    n = 5;
    q = &g_abWorldBalloonistState[5];
    do {
      *q = val;
      n -= 1;
      q -= 1;
    } while (n >= 0);
    PlaySoundEffect(LEVEL_SFX(SFX_REWARD), 0, SFX_MODE_PANNED, 0);
    break;
  case CHEAT_99_LIVES:
    g_nExtraLives = 99;
    break;
  }
}
