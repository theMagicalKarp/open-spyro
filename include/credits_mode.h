/* credits_mode.h — g_nCreditsMode, the GS_CREDITS state word. Below
 * CREDITS_MODE_RETURN_HOME the credits overlay runs (Credits_Update pages
 * the next scene in while the current one scrolls); from it on, main's
 * CreditsSequence_Update / ReturningHome_Draw take over. */

#ifndef OPEN_SPYRO_CREDITS_MODE_H
#define OPEN_SPYRO_CREDITS_MODE_H

enum CreditsMode {
  CREDITS_MODE_BUILD = -1,          /* BeginCreditsSequence: build the glyph pool, load page 0 */
  CREDITS_MODE_FETCH_HEADER = 0,    /* read the next page's 0x800-byte header */
  CREDITS_MODE_FETCH_TEXTURES = 1,  /* read its texture chunk */
  CREDITS_MODE_FETCH_SCENE = 2,     /* upload the strip, read the scene bundle */
  CREDITS_MODE_WAIT_SCROLL = 3,     /* wait for the current scroll track to end */
  CREDITS_MODE_SWAP_SCENE = 4,      /* upload + unpack the fetched scene */
  CREDITS_MODE_RETURN_HOME = 0x63,  /* pick the return level, start the stream */
  CREDITS_MODE_RETURN_HOME_STREAM = 0x64, /* "RETURNING HOME..." while it streams */
};

#endif /* OPEN_SPYRO_CREDITS_MODE_H */
