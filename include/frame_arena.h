/* frame_arena.h — the per-frame scratch arena at g_apMainDrawEntityList.
 * Kept free of other includes so headers with their own base types (the
 * titlescreen overlay's) can take it too. */

#ifndef OPEN_SPYRO_FRAME_ARENA_H
#define OPEN_SPYRO_FRAME_ARENA_H

/* Byte offset of g_apSpritePrimQueue in the arena; handlers holding the queue
 * pointer step back by it to reach the arena base. */
#define FRAME_ARENA_SPRITE_QUEUE 0x2400

/* Words of the arena below the sprite queue: the draw and sort lists every
 * frame's draw clears (FillWord) before rebuilding them. */
#define FRAME_ARENA_LIST_WORDS (FRAME_ARENA_SPRITE_QUEUE / 4)

#endif /* OPEN_SPYRO_FRAME_ARENA_H */
