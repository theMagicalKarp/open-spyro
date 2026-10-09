#include "globals.h"

/* Rebuilds the HUD sprite-record queue for the loading/inventory screen
   (0x80019300, 0x398). Appends, to the first free slot of
   g_apSpritePrimQueue: the five gem-counter icon records (starting at record
   4 when the counter is in state 4), the gem digit chain built on the fly by
   sprintf + BuildTextSpriteChain (each digit record gets a cosine bob written
   to +0x46), then three dragon records, three life records and the level-egg
   record, terminated with a NULL slot. Skipped wholesale on flight levels.

   Always (flight levels included) the sparx-food butterflies and the world-egg
   icons are emitted directly as textured quads: the butterflies get a swept
   cosine shade (one 1/20th-turn phase step per butterfly), the eggs cycle
   through nine UV pairs.

   The roll timer and both quad loops address their blocks through the
   global symbol, not the flag pointer: cse rewrites each constant address
   relative to whichever register already holds a related one, which is what
   puts the original's held bases (and loop.c's hoist order) in place. */
extern int sprintf(char *dst, char *fmt, int a, int b);
extern void *BuildTextSpriteChain(char *text, int *rect, int width, int flags);
extern void EmitTexturedQuad(byte *quad, byte *uv, int *color);

extern unsigned short g_anCosineLut[]; /* g_anSineLut + 0x80 (quarter turn) */

/* Distinct incomplete-array views of the five g_abHudCounterDisplayState
   bytes: each block loads its own flag at [0] and derives its record address
   off the same base. */
extern byte g_abHudGemCounterBlock[];      /* +0: icon records at +0x44 */
extern byte g_abHudGemRollBlock[];         /* +0: roll timer at +0xC */
extern byte g_abHudDragonCounterBlock[];   /* +1: records at +0x1FB */
extern byte g_abHudLivesCounterBlock[];    /* +2: records at +0x302 */
extern byte g_abHudButterflyRingBlock[];   /* +2: butterfly quads at +0x4C2 */
extern byte g_abHudWorldEggIconBlock[];    /* +3: egg quads at +0x461 */
extern byte g_abHudLevelEggCounterBlock[]; /* +4: record at +0x408 */

void EnqueueLoadingScreenSprites(void) {
  void **slot;
  byte *gems;
  byte *digits;
  byte *rec;
  byte *dragons;
  byte *lives;
  byte *eggs;
  byte *ring;
  byte *quad;
  int i;
  int phase;
  int shade;
  int step;
  int scratch[4]; /* sprintf text buffer / EmitTexturedQuad colour triple */
  int rect[3];
  int spare[4];

  if (g_nFlightLevelActive == 0) {
    slot = g_apSpritePrimQueue;
    if (*slot != 0) {
      slot += 1;
      while (*slot != 0) {
        slot += 1;
      }
    }
    gems = g_abHudGemCounterBlock;
    if (gems[0] != 0) {
      i = (gems[0] == 4) * 4;
      gems += 0x44;
      if (i < 5) {
        gems += i * 0x58;
        do {
          *slot = gems;
          slot += 1;
          i += 1;
          gems += 0x58;
        } while (i < 5);
      }
      digits = g_abHudGemRollBlock;
      if (digits[0] == 4) {
        sprintf((char *)scratch, g_szFmtDSlashD, g_nHudLevelGemCachedCount,
                g_nHudLevelGemCachedCount);
        rec = (byte *)g_pSpriteRecordWriteCursor;
        rect[0] = 0x5A;
        rect[1] = 0x24;
        rect[2] = 0xB40;
        BuildTextSpriteChain((char *)scratch, rect, 0x1C, 0xB);
        rec -= 0x58;
        phase = 0;
        while ((int)rec >= (int)g_pSpriteRecordWriteCursor) {
          rec[0x46] =
              g_anCosineLut[(*(int *)(g_abHudGemRollBlock + 0xC) * 4 + phase) &
                            0xFF] >>
              7;
          *slot = rec;
          slot += 1;
          rec -= 0x58;
          phase += 0xC;
        }
      }
    }
    dragons = g_abHudDragonCounterBlock;
    if (dragons[0] != 0) {
      dragons += 0x1FB;
      i = 0;
    dragon_slot:
      *slot = dragons;
      slot += 1;
      i += 1;
      dragons += 0x58;
      if (i < 3) {
        goto dragon_slot;
      }
    }
    lives = g_abHudLivesCounterBlock;
    if (lives[0] != 0) {
      i = 0;
      lives += 0x302;
    life_slot:
      *slot = lives;
      slot += 1;
      i += 1;
      lives += 0x58;
      if (i < 3) {
        goto life_slot;
      }
    }
    eggs = g_abHudLevelEggCounterBlock;
    if (eggs[0] != 0 && g_nHudLevelEggCachedCount == 1) {
      *slot = eggs + 0x408;
      slot += 1;
    }
    *slot = 0;
  }

  ring = g_abHudButterflyRingBlock;
  if (ring[0] != 0) {
    i = 0;
    if (g_nHudButterflyCachedCount > 0) {
      do {
        shade =
            ((short)
                 g_anCosineLut[(g_nLightSweepAngle - ((i << 8) / 20)) & 0xFF] >>
             7) +
            0x80;
        scratch[0] = shade;
        scratch[1] = shade;
        scratch[2] = shade;
        EmitTexturedQuad(g_abHudButterflyRingBlock + 0x4C2 + i * 8,
                         g_abHudButterflyRingBlock + 0x562, scratch);
        i += 1;
      } while (i < *(int *)(g_abHudButterflyRingBlock + 0x36));
    }
  }

  ring = g_abHudWorldEggIconBlock;
  if (ring[0] != 0) {
    i = 0;
    if (g_nHudWorldEggCachedCount > 0) {
      do {
        quad = g_abHudWorldEggIconBlock + 0x461 + i * 8;
        step = *(int *)(g_abHudWorldEggIconBlock + 0x3D) + i;
        EmitTexturedQuad(
            quad, (step % 9) * 8 + (g_abHudWorldEggIconBlock + 0x569), 0);
        i += 1;
      } while (i < *(int *)(g_abHudWorldEggIconBlock + 0x29));
    }
  }
}
