#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "rot_billboard.h"
#include "vecmath.h"

/* 0x8003aaec — allocate a particle record (g_aRotBillboards, 24-byte
   stride), seed its color to white, rotate the spawn offset by the
   actor's matrix into the record, add the actor position, then fill
   the lifetime/phase bytes. Returns the record index (or the negative
   allocation failure). */
int SpawnActorRotBillboard(Actor *actor, int *offset) {
  RotBillboard *rec;
  int idx;
  idx = FindFreeRotBillboardSlot();
  if (idx >= 0) {
    rec = (RotBillboard *)(idx * 24 + (int)g_aRotBillboards);
    rec->r = 0xFF;
    rec->g = 0xFF;
    rec->b = 0xFF;

    RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
    RotateVectorByMatrix(actor->mtx, offset, rec->pos);
    AddVector(rec->pos, rec->pos, &actor->posX);
    rec->angle = GetRandomU32();

    rec->angVel = 3;
    rec->life = 0x20;
    rec->lifePeak = 0x10;
    rec->halfSize = 0x20;
    rec->cullDepth = 0x40;
  }
  return idx;
}
