#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* 0x80059474 (0xF8) — latch `actor`'s position and facing as the
   level spawn anchor: store the wrapped yaw (<<4 & 0xFFF) and position, mark
   the anchor valid, bias spawn Z up 0x164, snapshot the level's rescued/gem
   tallies into the persistence block, merge the pending block into the
   committed one (copy 0x20 bytes then OR the 8 words), and carry the spawn
   flag word across. */
/* Two scheduling levers carry this function:
   - the anchor flag's `1` is carried by `t` (the snapshot temp) on purpose: as
     a bare literal its pseudo is single-set, so sched.c birthing_insn_p boosts
     the `li` to max_priority and it lands next to its store; sharing the
     already twice-set `t` makes it non-birthing and it drifts to the block
     front where the original has it (F14 carrier, 4 insns -> 2);
   - the merge source is spelled INLINE at the CopyWords call and the loop
     cursor `src` is assigned AFTER it (cse folds the two into the one
     `addiu s0,s0,-44`, but at the later insn's position), so the `move a0,dst`
     arg copy schedules ahead of it. Assigning `src` before the call — in any
     position, including at the call itself — always emits the addiu first. */

void SetSpawnAnchorFromActor(Actor *actor, int yaw) {
  int *rescued;
  int *dst;
  int *src;
  int i;
  int t;
  g_anLevelSpawnYawBlock[0] = (yaw << 4) & 0xFFF;
  CopyVector((int *)(&g_anLevelSpawnYawBlock[-3]), &actor->posX);
  t = 1;
  *((volatile int *)(&g_nSpawnAnchorValid)) = t;
  dst = &g_anLevelSpawnYawBlock[-19];
  *((volatile int *)(&g_nLevelSpawnPosZ)) += 0x164;
  rescued = &g_anLevelDragonsRescued[g_nLevelIndex];
  {
    t = *rescued;
    *((volatile int *)(&g_nSpawnAnchorDragonsRescued)) = t;
    t = g_anLevelGemsCollected[g_nLevelIndex];
    *(&g_nSpawnAnchorReserved) = 0;
    *((volatile int *)(&g_nSpawnAnchorGemsCollected)) = t;
  }
  CopyWords(dst, &g_anLevelSpawnYawBlock[-11], 0x20);
  src = &g_anLevelSpawnYawBlock[-11];
  i = 0;
  do {
    int v = *src;
    *dst = (*dst) | v;
    src += 1;
    i += 1;
    dst += 1;
  } while (i < 8);
  g_nLevelProgressCheckpoint = g_nLevelProgress;
}
