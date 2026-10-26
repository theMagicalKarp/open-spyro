#include "funcs.h"
#include "globals.h"
#include "level_id.h"
#include "level_sfx.h"
#include "main_shared.h"
#include "sample_bank.h"
#include "vecmath.h"

/* Begins the dragon-rescue statue cutscene overlay (GS_DRAGON_RESCUE): seeds
   the cutscene substate/timer globals, bumps both dragon counters (per-level +
   world total, priming the HUD cache), blocks gameplay, flags the linked
   actor's anim byte, anchors the aura star, and fires the rescue
   chime. Special-cases Artisans home (level 10 pickup 1->2) and Gnasty's Loot
   (level 0x3c key-locked treasure rewrite). (0x8002c924, 584 bytes)

   The shared literal 1 for the two flag stores is a `$4` register local: as a
   pseudo it outranks the `dragons` pointer in local-alloc and takes $v1, which
   pushes the dragons chain and world into the wrong registers. */
void BeginDragonRescue(Actor *actor) {
  unsigned int *st;
  int world;
  int *dragons;
  int prevDragons;
  int idx;
  register int one asm("$4");
  st = (unsigned int *)actor->state;
  if (((g_nActiveLevelId == LEVEL_ARTISANS) && (st[6] == 1)) &&
      ((((g_abLevelExited[1] != 0) || (g_abLevelExited[2] != 0)) ||
        (g_abLevelExited[3] != 0)) ||
       (g_abLevelExited[5] != 0))) {
    st[6] = 2;
  }
  if (g_nActiveLevelId == LEVEL_GNORC_GNEXUS) {
    if (g_abLevelExited[0x21] != 0) {
      st[6] = 1;
      st[0xa] = 0x800;
      st[0xb] = 0xcc8;
      st[2] = 0x2561;
      st[3] = 0xffc;
      st[4] = 0x6a;
      st[5] = 0xbb;
      st[0xc] = 0x800;
      st[0] = 0x134bf;
      st[1] = 0x13ce9;
      st[0xe] = 0x10;
    }
  }
  g_nGamestate = GS_DRAGON_RESCUE;
  world = g_nWorldDragonsRescued;
  g_nHudDragonCachedCount = world;
  dragons = &g_anLevelDragonsRescued[g_nLevelIndex];
  prevDragons = *dragons;
  one = 1;
  g_nGameplayBlocked = one;
  g_nGamePaused = one;
  *dragons = prevDragons + 1;
  g_anDragonRescueBlock[0] = 0;
  g_anDragonRescueBlock[1] = 0;  /* g_nDragonRescueStreamState */
  g_anDragonRescueBlock[5] = 0;  /* g_nDragonRescueTimer */
  g_anDragonRescueBlock[10] = 0; /* g_nDragonRescueSecondaryTimer */
  g_anDragonRescueBlock[13] = 0; /* g_nDragonRescuePadHoldTimer */
  g_anDragonRescueBlock[12] = 0; /* g_nDragonRescueAudioFiredFlag */
  g_anDragonRescueBlock[0x1a] = (int)actor;
  g_nWorldDragonsRescued = world + 1;
  g_anDragonRescueBlock[-1] = st[6]; /* g_nDragonRescueTypeSnapshot */
  if (st[8] != (-1)) {
    ((Actor *)g_pActorListBase)[st[8]].anim = 1;
  }
  CopyVector(g_anDragonRescueAuraStarAnchor, &actor->posX);
  g_anDragonRescueAuraStarAnchor[2] += 0x40;
  PlaySoundEffect(LEVEL_SFX(SFX_RESCUE_START),
                  (Actor *)&g_anDragonRescueBlock[0x1a], SFX_MODE_PANNED,
                  &((Actor *)g_anDragonRescueBlock[0x1a])->sfxSlot);
}
