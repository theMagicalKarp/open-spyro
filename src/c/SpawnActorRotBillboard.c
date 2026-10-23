#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

extern int FindFreeRotBillboardSlot();
extern void RefreshActorDerivedState();

typedef struct RotBillboard {
  int pos[3];             /* 0x00: world position */
  unsigned char life;     /* 0x0C: frames left; 0 = slot free */
  unsigned char lifePeak; /* 0x0D: size-envelope peak & fade divisor */
  unsigned char angle;    /* 0x0E: spin phase into the sine LUT */
  signed char angVel;     /* 0x0F: signed spin speed */
  unsigned char r, g, b;  /* 0x10: color (word is OR'd with poly code) */
  unsigned char pad13;
  unsigned char halfSize;  /* 0x14: base sprite half-size */
  unsigned char cullDepth; /* 0x15: max screen z, in units of 0x100 */
  unsigned char pad16[2];
} RotBillboard; /* 0x18 */

/* 0x8003aaec — allocate a particle record (24-byte stride table at
   0x80077108), seed its color to white, rotate the spawn offset by the
   actor's matrix into the record, add the actor anchor (+0xC), then fill
   the lifetime/phase bytes. Returns the record index (or the negative
   allocation failure). */
/* PARKED 2026-07-08 at 86.8% (7/53 insns): correct logic + store order;
   residue is the tail constant-sb block — original holds 0x20 in v1 with
   its li hoisted above the `3` store, and 0x40's li hoisted above the two
   0x20 stores. Named local, per-statement locals, and chained assignment
   all const-prop back to byte-identical per-use v0 materialization
   (InitActorRenderDefaults constant-sb scheduling class). Permuter
   candidate. */
int SpawnActorRotBillboard(int *actor, int *offset) {
  RotBillboard *rec;
  int idx;
  idx = FindFreeRotBillboardSlot();
  if (idx >= 0) {
    rec = (RotBillboard *)(idx * 24 + (int)g_aRotBillboards);
    rec->r = 0xFF;
    rec->g = 0xFF;
    rec->b = 0xFF;

    RefreshActorDerivedState(actor, 4);
    RotateVectorByMatrix(actor + 8, offset, rec->pos);
    AddVector(rec->pos, rec->pos, actor + 3);
    rec->angle = GetRandomU32();

    rec->angVel = 3;
    rec->life = 0x20;
    rec->lifePeak = 0x10;
    rec->halfSize = 0x20;
    rec->cullDepth = 0x40;
  }
  return idx;
}
