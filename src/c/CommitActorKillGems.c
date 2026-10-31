#include "actor_type.h"
#include "funcs.h"
#include "globals.h"

#define ACT ((Actor *)actor)

/* The sample-bank global is the head of the level's per-sound pointer table
   (the level overlays index the same object as a pointer array). Reading it
   as a RECORD MEMBER, not as a plain scalar, is load-bearing here: sched.c's
   true_dependence() drops the dependence between a MEM_IN_STRUCT store at a
   varying address (the kill-bitmap `row[word]` below) and a non-MEM_IN_STRUCT
   load at a fixed address, which lets the scheduler hoist the bank load above
   the store. A COMPONENT_REF sets MEM_IN_STRUCT_P on the load, the dependence
   holds, and the store keeps the original's position. */
struct SampleBankRef {
  unsigned char *head;
};

/* 0x8003b854 (0x180) — commit a kill/pickup for `actor` worth `value` gems:
   bank the gems (per-level tally + world treasure, with a low-health jingle
   check on the actor's +0x14 field), set the actor's bit in the committed-kill
   bitmap (by 0x58-byte pool index when the pointer is in the live pool, else
   by its +0x56 id byte) and in the per-level kill bitmap, then play the
   level's pickup sound on the actor. */
void CommitActorKillGems(int value, unsigned char *actor) {
  int id;
  int word;

  if (value != 0) {
    int *gems;
    int w;
    int g;
    if (ACT->posZ >= 0x300) {
      SpawnFloatingDigitActors(value, ACT);
    }
    gems = &g_anLevelGemsCollected[g_nLevelIndex];
    w = g_nWorldTreasureCollected + value;
    g = *gems + value;
    g_nWorldTreasureCollected = w;
    *gems = g;
  }

  if ((unsigned int)actor >= (unsigned int)g_pActorListBase &&
      (unsigned int)actor < (unsigned int)g_pActorPoolDynBase) {
    int raw;
    id = ACT - (Actor *)g_pActorListBase;
    word = id >> 5;
    raw = id;
    id = id & 0x1F;
    g_anCommittedKillBitmap[word] |= 1 << id;
    id = raw;
  } else {
    id = ((Actor *)actor)->poolIndex;
  }

  word = id >> 5;
  id = id & 0x1F;
  {
    int *row = &g_anLevelKillBitmapTable[g_nLevelIndex * 8];
    row[word] |= 1 << id;
  }
  PlaySoundEffect(*((struct SampleBankRef *)&g_pLevelSampleBankHeader)->head,
                  (int)actor, SFX_MODE_PANNED, 0);
}
