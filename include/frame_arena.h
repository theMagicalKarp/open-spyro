/* frame_arena.h — the per-frame scratch arena at g_apMainDrawEntityList.
 * Kept free of other includes so headers with their own base types (the
 * titlescreen overlay's) can take it too. */

#ifndef OPEN_SPYRO_FRAME_ARENA_H
#define OPEN_SPYRO_FRAME_ARENA_H

/* Words of the arena below g_apSpritePrimQueue (+0x2400): the draw and sort
 * lists every frame's draw clears (FillWord) before rebuilding them. */
#define FRAME_ARENA_LIST_WORDS 0x900

#endif /* OPEN_SPYRO_FRAME_ARENA_H */
