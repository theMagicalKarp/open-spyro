/* Native companions to src/main/gamestate/update.c definitions. */

#include "globals.h"
#include "pad_buttons.h"

void RestartTitleCardCheckOnTriangle(void);

/* The titlescreen overlay tests the result of RestartTitleCardCheckOnTriangle,
 * a void function: on PS1 it reads the $v0 the function leaves, which is the
 * pad test (0) when Triangle is not pressed, else the return of the blocking
 * MemCardSync(0, ...) it ends with (1: the command finished; it never returns
 * 0 in blocking mode). titlescreen.h routes the overlay's calls here. */
int Native_RestartTitleCardCheckOnTriangle(void) {
  if (!(g_dwPadPressed & PAD_TRIANGLE)) {
    return 0;
  }
  RestartTitleCardCheckOnTriangle();
  return 1;
}
