#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "pad_buttons.h"
#include "title_state.h"

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
