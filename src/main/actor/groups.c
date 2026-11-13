#include "actor.h"
#include "actor_type.h"
#include "emit_record.h"
#include "funcs.h"
#include "globals.h"
#include "vecmath.h"

/* Scan pickup group `group`'s actor-id list (0x8003b0dc). Each entry is a u16:
   low 15 bits index the actor pool, bit 15 marks the last entry. Returns 1 as
   soon as one of the group's actors is not dormant (mode < ACTOR_MODE_DORMANT),
   else 0 (also when the group index is out of range). */
int PickupGroupHasLiveActor(int group) {
  unsigned short *ptr;
  unsigned short id;
  int idx;
  Actor *base;

  if (group < g_nLevelPickupTableCount) {
    ptr = g_pLevelPickupTableEntries[group];
    base = g_pActorListBase;
    do {
      id = *ptr;
      idx = id & 0x7FFF;
      if ((unsigned int)base[idx].mode < ACTOR_MODE_DORMANT) {
        return 1;
      }
      ptr++;
    } while ((short)id >= 0);
  }
  return 0;
}

/* Scan pickup group `group`'s actor-id list (0x8003b160). Each entry is a u16:
   low 15 bits index the actor pool, bit 15 marks the last entry. Returns 0 as
   soon as one of the group's actors is not in `mode`; returns 1 if every actor
   is (also 0 when the group index is out of range). Nothing calls it.
   Unreferenced: no call, branch, pointer or table entry in main or any overlay
   (xref). */
int IsPickupGroupAllInMode(int group, int mode) {
  unsigned short *ptr;
  unsigned short id;
  int idx;
  Actor *base;

  if (group >= g_nLevelPickupTableCount) {
    return 0;
  }
  ptr = g_pLevelPickupTableEntries[group];
  base = g_pActorListBase;
  do {
    id = *ptr;
    idx = id & 0x7FFF;
    if (base[idx].mode != mode) {
      return 0;
    }
    ptr++;
  } while ((short)id >= 0);
  return 1;
}

/* Set ->mode to `value` for every actor in the pickup group
   named by actor's ->group whose current mode is < 0x80
   (0x8003b1e8). If that group index is out of range, apply the same guarded
   write to the passed actor itself. */
void SetActorGroupState(Actor *actor, int value) {
  unsigned short *ptr;
  int idx;
  int group;

  group = actor->group;
  if (group >= g_nLevelPickupTableCount) {
    if ((unsigned int)actor->mode < ACTOR_MODE_DORMANT) {
      actor->mode = value;
    }
  } else {
    ptr = g_pLevelPickupTableEntries[group];
    do {
      idx = *ptr & 0x7FFF;
      if ((unsigned int)g_pActorListBase[idx].mode < ACTOR_MODE_DORMANT) {
        g_pActorListBase[idx].mode = value;
      }
    } while ((short)*ptr++ >= 0);
  }
}

/* OR `bits` into the +0x18 flags word of every actor in pickup group `group`
   that lies inside a box + cone around actor `actor` (0x8003b294): per-axis
   |dx|,|dy| < xyLim, ApproxDist2D < distLim, |dz| < zLim, and the 8-bit
   heading from `actor` to the member within angLim of actor's facing byte
   (rotZ). Dormant members (mode >= ACTOR_MODE_DORMANT) are skipped. */

void OrActorGroupFlagsInCone(Actor *actor, int group, unsigned int bits,
                             int distLim, int zLim, int xyLim, int angLim) {
  unsigned short *ptr;
  int idx;
  Actor *rec;
  int d;

  if (group < g_nLevelPickupTableCount) {
    ptr = g_pLevelPickupTableEntries[group];
    do {
      idx = *ptr & 0x7FFF;
      rec = &g_pActorListBase[idx];
      if (rec->mode < ACTOR_MODE_DORMANT) {
        d = actor->posX - rec->posX;
        if (d > 0) {
          if (d >= xyLim) {
            continue;
          }
          goto xdone;
        }
        if (rec->posX - actor->posX < xyLim) {
          goto xdone;
        }
        continue;
      xdone:
        d = actor->posY - rec->posY;
        if (d > 0) {
          if (d < xyLim) {
            goto ydone;
          }
          continue;
        }
        if (rec->posY - actor->posY >= xyLim) {
          continue;
        }
      ydone:
        if (ApproxDist2D(&actor->posX, &rec->posX) >= distLim) {
          continue;
        }
        d = rec->posZ - actor->posZ;
        if (d > 0) {
          if (d < zLim) {
            goto zdone;
          }
          continue;
        }
        if (actor->posZ - rec->posZ >= zLim) {
          continue;
        }
      zdone:
        if (AbsAngleDelta8(actor->rotZ, ArcTan2(rec->posX - actor->posX,
                                                rec->posY - actor->posY,
                                                ATAN_ANGLE8)) < angLim) {
          rec->flags |= bits;
        }
      }
    } while ((short)*ptr++ >= 0);
  }
}

/* Set ->mode to `value` for every actor in the pickup group
   named by actor's ->group whose current mode is live (< ACTOR_MODE_DORMANT)
   and not equal to `match` (0x8003b47c). Out-of-range group index applies the
   same guarded write to the passed actor itself. */
void SetActorGroupStateExcept(Actor *actor, int value, int match) {
  unsigned short *ptr;
  int idx;
  int group;
  unsigned int mode;

  group = actor->group;
  if (group >= g_nLevelPickupTableCount) {
    mode = actor->mode;
    if (mode < ACTOR_MODE_DORMANT && mode != match) {
      actor->mode = value;
    }
  } else {
    ptr = g_pLevelPickupTableEntries[group];
    do {
      idx = *ptr & 0x7FFF;
      mode = g_pActorListBase[idx].mode;
      if (mode < ACTOR_MODE_DORMANT && mode != match) {
        g_pActorListBase[idx].mode = value;
      }
    } while ((short)*ptr++ >= 0);
  }
}

/* Set ->mode to `value` for every actor in the pickup group
   named by actor's ->group whose current mode is live (< ACTOR_MODE_DORMANT)
   and equal to `match` (0x8003b538). Out-of-range group index applies the same
   guarded write to the passed actor itself. */
void ReplaceActorGroupState(Actor *actor, int value, int match) {
  unsigned short *ptr;
  int idx;
  int group;
  unsigned int mode;

  group = actor->group;
  if (group >= g_nLevelPickupTableCount) {
    mode = actor->mode;
    if (mode < ACTOR_MODE_DORMANT && mode == match) {
      actor->mode = value;
    }
  } else {
    ptr = g_pLevelPickupTableEntries[group];
    do {
      idx = *ptr & 0x7FFF;
      mode = g_pActorListBase[idx].mode;
      if (mode < ACTOR_MODE_DORMANT && mode == match) {
        g_pActorListBase[idx].mode = value;
      }
    } while ((short)*ptr++ >= 0);
  }
}

/* Walk pickup group `group`'s actor-id list (0x8003b5f4); for every actor not
   dormant (mode < ACTOR_MODE_DORMANT), OR `bits` into its flags. The |= store
   may alias the id list, so the bit-15 last-entry test reloads *ptr as a signed
   halfword. No-op when the group index is out of range. */
void OrPickupGroupActorFlags(int group, int bits) {
  unsigned short *ptr;
  int idx;
  Actor *base;

  if (group < g_nLevelPickupTableCount) {
    ptr = g_pLevelPickupTableEntries[group];
    base = g_pActorListBase;
    do {
      idx = *ptr & 0x7FFF;
      if ((unsigned int)base[idx].mode < ACTOR_MODE_DORMANT) {
        base[idx].flags |= bits;
      }
    } while ((short)*ptr++ >= 0);
  }
}

/* Walk pickup group `a0`'s actor-id list (0x8003b688) and despawn every actor
   whose mode is < ACTOR_MODE_DORMANT (not dormant). The call clobbers the id
   load, so the bit-15 last-entry test reloads *ptr as a signed halfword. No-op
   when the group index is out of range. */
void DespawnPickupGroupActors(int a0) {
  unsigned short *ptr;
  int idx;

  if (a0 < g_nLevelPickupTableCount) {
    ptr = g_pLevelPickupTableEntries[a0];
    do {
      idx = *ptr & 0x7FFF;
      if ((unsigned int)g_pActorListBase[idx].mode < ACTOR_MODE_DORMANT) {
        DespawnActorRecord(&g_pActorListBase[idx]);
      }
    } while ((short)*ptr++ >= 0);
  }
}

/* Set ->subMode to `value` for every actor in the pickup group named by
   actor's ->group whose ->mode is < 0x80
   (0x8003b728). If that group index is out of range, just stamp the passed
   actor's own subMode instead. */
void SetActorGroupSubstate(Actor *actor, int value) {
  unsigned short *ptr;
  int idx;
  int group;

  group = actor->group;
  if (group >= g_nLevelPickupTableCount) {
    actor->subMode = value;
  } else {
    ptr = g_pLevelPickupTableEntries[group];
    do {
      idx = *ptr & 0x7FFF;
      if ((unsigned int)g_pActorListBase[idx].mode < ACTOR_MODE_DORMANT) {
        g_pActorListBase[idx].subMode = value;
      }
    } while ((short)*ptr++ >= 0);
  }
}

/* Mark `actor` in the committed-kill bitmap (0x8003b7c0). Ignores pointers
   outside the live pool [g_pActorListBase, g_pActorPoolDynBase); otherwise sets
   the bit for its pool index. */
void MarkActorLiveKill(Actor *actor) {
  int index;
  int word;
  int bit;

  if (actor >= g_pActorListBase && actor < g_pActorPoolDynBase) {
    index = actor - g_pActorListBase;
    word = index >> 5;
    bit = index & 0x1F;
    g_anLiveKillBitmap[word] |= 1 << bit;
  }
}

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
   bank the gems (per-level tally + world treasure; floating digits show the
   value unless the actor is below posZ 0x300), set the actor's bit in the
   committed-kill bitmap (by 0x58-byte pool index when the pointer is in the
   live pool, else by its +0x56 id byte) and in the per-level kill bitmap, then
   play the level's pickup sound on the actor. */
void CommitActorKillGems(int value, Actor *actor) {
  int id;
  int word;

  if (value != 0) {
    int *gems;
    int w;
    int g;
    if (actor->posZ >= 0x300) {
      SpawnFloatingDigitActors(value, actor);
    }
    gems = &g_anLevelGemsCollected[g_nLevelIndex];
    w = g_nTotalTreasureCollected + value;
    g = *gems + value;
    g_nTotalTreasureCollected = w;
    *gems = g;
  }

  if (actor >= g_pActorListBase && actor < g_pActorPoolDynBase) {
    int raw;
    id = actor - g_pActorListBase;
    word = id >> 5;
    raw = id;
    id = id & 0x1F;
    g_anLiveKillBitmap[word] |= 1 << id;
    id = raw;
  } else {
    id = actor->poolIndex;
  }

  word = id >> 5;
  id = id & 0x1F;
  {
    int *row = &g_anLevelKillBitmapTable[g_nLevelIndex * 8];
    row[word] |= 1 << id;
  }
  PlaySoundEffect(*((struct SampleBankRef *)&g_pLevelSampleBankHeader)->head,
                  actor, SFX_MODE_PANNED, 0);
}

/* 0x8003b9d4 (0xFC) — on picking up `actor` (a fodder/pickup): record its
   kind byte in the gem-tally icon UV ring, bump the ring count, special-case
   the locked chest's reward (g_pLockedChestReward -> key state 3), notify the
   pickup handler, spawn the level's particle burst for the kind, then queue the
   matching sound effect for kinds 0x53..0x57. */
void CollectPickupActor(Actor *actor) {
  short kind;
  int pad[4];

  g_abGemTallyIconUvTable[g_nGemTallyIconCount & 0x1F] = actor->type;
  g_nGemTallyIconCount += 1;
  if (actor == g_pLockedChestReward) {
    g_nLevelKeyState = KEYSTATE_REWARD_TAKEN;
  }
  RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
  g_pfnLevelEmitParticles(6, EMIT_ACTOR_SPARK, actor,
                          g_adwActorBurstColors[actor->type]);

  kind = actor->type;
  if (kind == ACTOR_TYPE_GEM_1) {
    CommitActorKillGems(1, actor);
  } else if (kind == ACTOR_TYPE_GEM_2) {
    CommitActorKillGems(2, actor);
  } else if (kind == ACTOR_TYPE_GEM_5) {
    CommitActorKillGems(5, actor);
  } else if (kind == ACTOR_TYPE_GEM_10) {
    CommitActorKillGems(0xA, actor);
  } else if (kind == ACTOR_TYPE_GEM_25) {
    CommitActorKillGems(0x19, actor);
  }
}

extern int g_anFlightTargetTally[];

/* Bumps the per-id usage counter g_anFlightTargetTally[id] and registers `id`
   into the 4-slot active table g_anFlightTallyActiveSlots (claiming the first
   free slot, marked by a negative entry; no-op if already present or the table
   is full). (0x8003cac4, 96 bytes.) */
void TallyFlightTarget(int id) {
  int i;

  g_anFlightTargetTally[id]++;
  for (i = 0; i < 4; i++) {
    if (g_anFlightTallyActiveSlots[i] < 0) {
      g_anFlightTallyActiveSlots[i] = id;
      return;
    }
    if (g_anFlightTallyActiveSlots[i] == id) {
      return;
    }
  }
}
