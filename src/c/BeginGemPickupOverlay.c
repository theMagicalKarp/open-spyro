#include "globals.h"

extern void CopyVector(int *dst, int *src);
extern int PlaySoundEffect(unsigned int id, int owner, unsigned int flags,
                           unsigned char *pos);

extern int g_anGemPickupBlock[]; /* held-base alias @ g_nGemPickupSubstate */

/* Begins the dragon-rescue statue cutscene overlay (GS_GEM_PICKUP): seeds the
   cutscene substate/timer globals, bumps both dragon counters (per-level +
   world total, priming the HUD cache), blocks gameplay, flags the source
   actor's render record at +0x3c, anchors the aura star, and fires the rescue
   chime. Special-cases Artisans home (level 10 pickup 1->2) and Gnasty's Loot
   (level 0x3c key-locked treasure rewrite). (0x8002c924, 584 bytes)

   The shared literal 1 for the two flag stores is a `$4` register local: as a
   pseudo it outranks the `dragons` pointer in local-alloc and takes $v1, which
   pushes the dragons chain and world into the wrong registers. */
void BeginGemPickupOverlay(unsigned int *param_1) {
  unsigned int *puVar1;
  int world;
  int *dragons;
  int prevDragons;
  int idx;
  register int one asm("$4");
  puVar1 = (unsigned int *)(*param_1);
  if (((g_nActiveLevelId == 0xa) && (puVar1[6] == 1)) &&
      ((((g_abLevelGemPercent[1] != 0) || (g_abLevelGemPercent[2] != 0)) ||
        (g_abLevelGemPercent[3] != 0)) ||
       (g_abLevelGemPercent[5] != 0))) {
    puVar1[6] = 2;
  }
  if (g_nActiveLevelId == 0x3c) {
    if (g_abLevelGemPercent[0x21] != 0) {
      puVar1[6] = 1;
      puVar1[0xa] = 0x800;
      puVar1[0xb] = 0xcc8;
      puVar1[2] = 0x2561;
      puVar1[3] = 0xffc;
      puVar1[4] = 0x6a;
      puVar1[5] = 0xbb;
      puVar1[0xc] = 0x800;
      puVar1[0] = 0x134bf;
      puVar1[1] = 0x13ce9;
      puVar1[0xe] = 0x10;
    }
  }
  g_nGamestate = 8;
  world = g_nWorldDragonsRescued;
  g_nHudDragonCachedCount = world;
  dragons = &g_anLevelDragonsRescued[g_nLevelIntroIndex];
  prevDragons = *dragons;
  one = 1;
  g_nGameplayBlocked = one;
  g_nGamePaused = one;
  *dragons = prevDragons + 1;
  g_anGemPickupBlock[0] = 0;
  g_anGemPickupBlock[1] = 0;  /* g_nGemPickupStreamState */
  g_anGemPickupBlock[5] = 0;  /* g_nGemPickupTimer */
  g_anGemPickupBlock[10] = 0; /* g_nGemPickupSecondaryTimer */
  g_anGemPickupBlock[13] = 0; /* g_nGemPickupPadHoldTimer */
  g_anGemPickupBlock[12] = 0; /* g_nGemPickupAudioFiredFlag */
  g_anGemPickupBlock[0x1a] = (int)param_1;
  g_nWorldDragonsRescued = world + 1;
  g_anGemPickupBlock[-1] = puVar1[6]; /* g_nGemPickupTypeSnapshot */
  if (puVar1[8] != (-1)) {
    *((unsigned char *)((((char *)g_pActorListBase) + (puVar1[8] * 0x58)) +
                        0x3c)) = 1;
  }
  CopyVector(g_anGemPickupAuraStarAnchor, (int *)(param_1 + 3));
  g_anGemPickupAuraStarAnchor[2] += 0x40;
  PlaySoundEffect(
      *((unsigned char *)(((char *)g_pLevelSampleBankHeader) + 0x30)),
      (int)(&g_anGemPickupBlock[0x1a]), 0x10,
      (unsigned char *)(((char *)g_anGemPickupBlock[0x1a]) + 0x54));
}
