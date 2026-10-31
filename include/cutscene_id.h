/* cutscene_id.h — g_nCutsceneId, the cutscene bundle TickCutsceneBundleLoadStream
 * streams in and Cutscene_Update plays (BeginCutscene). */

#ifndef OPEN_SPYRO_CUTSCENE_ID_H
#define OPEN_SPYRO_CUTSCENE_ID_H

enum CutsceneId {
  CUTSCENE_TITLE_BACKDROP = 0, /* the title screen's backdrop */
  CUTSCENE_OPENING = 1,        /* the opening story (Cross/Start skips) */
  CUTSCENE_GNASTY_ENDING = 2,  /* after Gnasty Gnorc is beaten (level 33) */
  CUTSCENE_LOOT_ENDING = 3,    /* leaving Gnasty's Loot (level 34) */
};

#endif /* OPEN_SPYRO_CUTSCENE_ID_H */
