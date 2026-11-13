#include "actor_states.h"
#include "actor_type.h"
#include "emit_record.h"
#include "funcs.h"
#include "globals.h"
#include "level_sfx.h"
#include "main_shared.h"
#include "rot_billboard.h"
#include "sample_bank.h"
#include "spyro_state.h"
#include "vecmath.h"

/* Play voice `voice` of the actor's model: the sample id is byte 4 + voice
   of its g_apActorMeshTable record. With a `voiceRow` slot the sound keys to
   it; otherwise the actor's own voices are stopped first and it keys to the
   actor's ->voiceRow. */
void PlayActorMeshVoice(Actor *actor, int voice, void *voiceRow) {
  if (voiceRow != 0) {
    PlaySoundEffect(
        ((unsigned char *)(&g_apActorMeshTable)[actor->type] + voice)[4], actor,
        SFX_MODE_3D, voiceRow);
  } else {
    StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
    PlaySoundEffect(
        ((unsigned char *)(&g_apActorMeshTable)[actor->type] + voice)[4], actor,
        SFX_MODE_3D, &actor->voiceRow);
  }
}

/* Play the actor's voice sample (sample id at sample-bank +0x26): a live actor
   (actor != 0) keys the one-shot to itself (SFX_MODE_3D, owner key
   &actor->voiceRow); the null actor plays it positionless (SFX_MODE_PANNED).
   Then run BurstActorIntoSparks, whose sparks live `sparkLife` (+ 0..3) frames.
 */
void BurstActorWithSound(Actor *actor, int sparkLife) {
  if (actor != 0) {
    PlaySoundEffect(LEVEL_SFX(SFX_ACTOR_BURST), actor, SFX_MODE_3D,
                    &actor->voiceRow);
  } else {
    PlaySoundEffect(LEVEL_SFX(SFX_ACTOR_BURST), 0, SFX_MODE_PANNED, (void *)0);
  }
  BurstActorIntoSparks(actor, sparkLife);
}

/* Step an actor's shade band and rebuild its tint word from it (0x8003a9ec).
   `band` 0 with no hit flags means "leave it alone"; otherwise the band is
   pushed out by 0x10 (up to 0x50) while the flame-hit bit is set and eased
   back in by 2 above 0x20, by 1 below. The quarter band n then becomes the
   tagged tint 0xA << 20 | (0x18 + n) << 12 | (0x18 - n) << 6 | (0x18 - n)
   (see Actor.tint0): the flame level rises as the other two fall. Returns
   the adjusted band. */
int StepActorScorchTint(Actor *actor, int band) {
  int n;
  int tint;

  if (band != 0 || actor->flags != 0) {
    if ((actor->flags & ACTOR_HIT_FLAME) && band < 0x50) {
      band += 0x10;
    } else if (band >= 0x21) {
      band -= 2;
    } else if (band > 0) {
      band -= 1;
    }
    n = band >> 2;
    tint = 0xA00000 + ((n + 0x18) << 12) + ((0x18 - n) << 6) + 0x18;
    *(int *)&actor->tint0 = tint - n;
  }
  return band;
}

/* Tick the actor's tint2 glow byte. While ACTOR_HIT_FLAME is set in ->flags
   it counts UP by 0x18, wrapping back to 0x60 once it reaches 0x61. When
   the flag is clear it counts DOWN: by 2 while >= 0x21, by 1 below that, and
   holds at 0. Named for the flag: 0x10000 is the flame-hit bit (a flame
   breaks the type-146 prop at once) and StepActorScorchTint pushes the shade
   band out for the same flag, so tint2 reads as a scorch glow (likely). */
void TickActorFlameGlow(Actor *actor) {
  if (actor->flags & ACTOR_HIT_FLAME) {
    int v = actor->tint2 + 0x18;
    actor->tint2 = v;
    if ((unsigned char)v >= 0x61) {
      actor->tint2 = 0x60;
    }
  } else {
    unsigned char b;
    if ((b = actor->tint2) >= 0x21) {
      actor->tint2 = b - 2;
    } else if (b != 0) {
      actor->tint2 = b - 1;
    }
  }
}

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

/* The tumbling sub-record hung off the drop's ->state pointer. */
struct Debris {
  int vel[3];                        /* 0x00 */
  char pad0c[2];                     /* 0x0c */
  unsigned char live;                /* 0x0e */
  char pad0f[2];                     /* 0x0f */
  unsigned char spinX, spinY, spinZ; /* 0x11 */
};

/* Spawn the drop that a broken/flamed actor leaves behind (0x8003abc0,
   1308 bytes).  `src` is the actor that died; `kind` selects how the drop is
   thrown.  An actor whose dropKind (low 7 bits) is 1..0x7e drops that actor
   type, once (killGroup bit 7 latches it).  Any other actor rolls 0..99: 2%
   an extra life (ACTOR_TYPE_EXTRA_LIFE); else, while g_nSparxHealth <
   SPARX_GOLD, 10% a butterfly (ACTOR_TYPE_BUTTERFLY), or Sparx himself
   when he is gone (no g_pSparxActor) and the health is not negative; else a
   Sparx food (ACTOR_TYPE_SPARX_FOOD).  Extra lives, butterflies and Sparx are
   returned as spawned.  Anything else is thrown by `kind` (enum DropThrow,
   actor_type.h): DROP_HOP / DROP_AT_POINT land on the source (or `alt`)
   position, DROP_HIGH_HOP with a small XY jitter, DROP_SCATTER and a
   DROP_TUMBLE_IF_NEAR that fails its Spyro proximity test scatter over a
   wider XY box, DROP_IN_PLACE drops in place with zero velocity, and
   DROP_TUMBLE (or a passing DROP_TUMBLE_IF_NEAR) rides off the source with a
   random tumble.  The thrown path probes the landing spot for ground
   and a walkable slope, integrates the ballistic arc at DROP_GRAVITY
   per frame to count the flight time, and divides the offset by that count to
   get the launch velocity. */
Actor *SpawnActorDeathDrop(Actor *src, int kind, int *hint, int *alt) {
  int land[4];   /* sp+0x18 */
  int throw_[4]; /* sp+0x28 */
  Actor *rec;
  int vz;
  struct Debris *dbg;
  int ang;
  int id;
  int n;
  int hit;
  int z;
  int v;
  int d;
  int e;

  id = src->dropKind & 0x7f;
  if ((unsigned int)(id - 1) < 0x7e) {
    if (src->killGroup & ACTOR_KILL_DROPPED) {
      return 0;
    }
    src->killGroup |= ACTOR_KILL_DROPPED;
  } else {
    int roll;
    id = ACTOR_TYPE_SPARX_FOOD;
    roll = (int)(((GetRandomU32() & 0xfff) * 25) >> 10); /* 0..99 */
    if (roll < DROP_LIFE_PERCENT) {
      id = ACTOR_TYPE_EXTRA_LIFE;
    } else if (g_nSparxHealth < SPARX_GOLD && roll < DROP_BUTTERFLY_PERCENT) {
      id = ACTOR_TYPE_BUTTERFLY;
    }
  }

  if (id == ACTOR_TYPE_BUTTERFLY) {
    if (g_pSparxActor != 0 || g_anSparxHealthBlock[0] < SPARX_GONE) {
      rec = g_pfnLevelSpawnActor(ACTOR_TYPE_BUTTERFLY, src);
    } else {
      rec = g_pfnLevelSpawnActor(ACTOR_TYPE_SPARX, src);
      PlayActorMeshVoice(rec, SPARX_VOICE_BUZZ, 0);
      g_anSparxHealthBlock[0] = SPARX_GREEN;
      g_pSparxActor = rec;
    }
    return rec;
  }

  if (id == ACTOR_TYPE_EXTRA_LIFE) {
    rec = g_pfnLevelSpawnActor(ACTOR_TYPE_EXTRA_LIFE, src);
    dbg = (struct Debris *)rec->state;
    dbg->live = 1;
    return rec;
  }

  rec = g_pfnLevelSpawnActor(id, src);
  dbg = (struct Debris *)rec->state;

  if (kind == DROP_HOP) {
    CopyVector(throw_, &src->posX);
    vz = DROP_HOP_VZ;
  } else if (kind == DROP_HIGH_HOP) {
    CopyVector(throw_, &src->posX);
    vz = DROP_HIGH_HOP_VZ;
    throw_[0] -= DROP_HIGH_HOP_JITTER -
                 (int)(GetRandomU32() & (2 * DROP_HIGH_HOP_JITTER - 1));
    throw_[1] -= DROP_HIGH_HOP_JITTER -
                 (int)(GetRandomU32() & (2 * DROP_HIGH_HOP_JITTER - 1));
  } else if (kind == DROP_SCATTER) {
    CopyVector(throw_, &src->posX);
    vz = DROP_HOP_VZ;
    throw_[0] -= DROP_SCATTER_JITTER -
                 (int)(GetRandomU32() & (2 * DROP_SCATTER_JITTER - 1));
    throw_[1] -= DROP_SCATTER_JITTER -
                 (int)(GetRandomU32() & (2 * DROP_SCATTER_JITTER - 1));
  } else if (kind == DROP_AT_POINT) {
    CopyVector(throw_, alt);
    vz = DROP_HOP_VZ;
  } else if (kind == DROP_TUMBLE_IF_NEAR || kind == DROP_TUMBLE) {
    if ((kind == DROP_TUMBLE || g_nSpyroState == SPYRO_ST_CHARGE ||
         g_nSpyroState == SPYRO_ST_SUPERCHARGE_AIR ||
         g_nSpyroState == SPYRO_ST_CHARGE_AIR ||
         g_nSpyroState == SPYRO_ST_SUPERCHARGE) &&
        (kind == DROP_TUMBLE ||
         (ApproxDist2D(&rec->posX, g_anSpyroWorldPos) < DROP_TUMBLE_RANGE &&
          (d = rec->posZ - rec->floorOffset, e = d - g_nSpyroWorldPosZ,
           e > 0 ? e < DROP_TUMBLE_DZ
                 : g_nSpyroWorldPosZ - d < DROP_TUMBLE_DZ)))) {
      int *p = &rec->posX;
      CopyVector(p, &src->posX);
      rec->posZ += DROP_LIFT;
      CopyVector(dbg->vel, p);
      dbg->spinX = GetRandomU32() & 0xe;
      dbg->spinY = GetRandomU32() & 0xe;
      dbg->spinZ = GetRandomU32() & 0xe;
      rec->subMode = COLLECT_HOMING;
      rec->updateDist = ACTOR_UPDATE_ALWAYS;
      return rec;
    }
    CopyVector(throw_, &src->posX);
    vz = DROP_HOP_VZ;
    throw_[0] -= DROP_SCATTER_JITTER -
                 (int)(GetRandomU32() & (2 * DROP_SCATTER_JITTER - 1));
    throw_[1] -= DROP_SCATTER_JITTER -
                 (int)(GetRandomU32() & (2 * DROP_SCATTER_JITTER - 1));
  } else if (kind == DROP_IN_PLACE) {
    CopyVector(&rec->posX, &src->posX);
    rec->posZ += DROP_LIFT;
    ZeroVector(dbg->vel);
    return rec;
  }

  if (hint != 0) {
    CopyVector(land, hint);
  } else {
    CopyVector(land, &src->posX);
    land[2] += DROP_LIFT;
  }

  throw_[2] += GROUND_PROBE_LIFT;
  hit = FindGroundHeightBelow(throw_, 0x800);
  throw_[2] -= 0x400;
  ang = (signed char)ArcTan2_8bit(
      g_anCollisionHitNormal[2],
      VectorLength(g_anCollisionHitNormal, VEC_LEN_XY));
  if (hit == 0 || ang >= DROP_MAX_SLOPE ||
      DispatchActorContactAtSphere(throw_, DROP_CLEAR_RADIUS, 0, 0, 0,
                                   CONTACT_STOP_AT_SOLID) != 0) {
    CopyVector(throw_, &src->posX);
  }
  n = 0;

  z = land[2];
  v = vz;
  while (v > 0 || throw_[2] < z) {
    z += v;
    v -= DROP_GRAVITY;
    n += 1;
  }

  CopyVector(&rec->posX, land);
  SubtractVector(throw_, throw_, land);
  DivideVector3(throw_, throw_, n);
  throw_[2] = vz;
  CopyVector(dbg->vel, throw_);
  dbg->live = 1;
  return rec;
}

/* The letter's NameLetterState pointer (->state; a sign actor's ->state holds
   its level id instead) is re-read before every field store (the original
   never proves the letter actor's +0 stays put across them). */
#define REC(p) (*(NameLetterState *volatile *)&(p)->state)

/* Spells a name out in floating letter actors around `sign`. With `named` set
   (NAME_LETTERS_LEVEL) the text is the level name for the sign's own level id
   and the arc hangs off the sign's yaw (mirrored, and flipped through the
   origin, when the sign's ->subMode is non-zero); otherwise it is "RETURN
   HOME" (slot 0 of g_apBalloonistNames, ahead of the per-world names) laid out
   facing Spyro, as the level-exit whirlwinds show it. Letters are spawned
   through g_pfnLevelSpawnActor ('A'-'Z'
   map to id 0x1AA.., anything else to 0x4C) and threaded onto a radial arc:
   each glyph sits one `spacing` step further round the ring, bent outward by
   `bend` and lifted by the cosine of its position ('L' actors get an extra
   0x100 of height). Spaces advance the cursor without spawning.
   (0x8003c358, 908 bytes.) */
void SpawnNameLetterActors(Actor *sign, int named) {
  /* Four 16-byte (VECTOR-shaped) slots at sp+0x10/0x20/0x30/0x40, plus 8 bytes
     of tail slack the original's frame reserves and never touches. */
  int radial[4];
  int spacing[4];
  int cursor[4];
  int bend[4];
  int scratch[2];
  char *name;
  int yaw;
  int sideAng;
  int endAng;
  int ang;
  int step;
  int len;
  int n;
  int i;
  char *p;
  Actor *letter;
  int *pos;

  if (named != 0) {
    name = g_apLevelNameStrings[*(int *)sign->state];
    yaw = sign->rotZ;
  } else {
    name = g_apBalloonistNames[0];
    yaw = ArcTan2(g_anSpyroWorldPos[0] - sign->posX,
                  g_nSpyroWorldPosY - sign->posY, ATAN_ANGLE8);
  }
  sideAng = yaw & 0xff;
  ang = sideAng * 16;
  radial[0] = (LookupCosine(ang) * 3) >> 4;
  radial[1] = (LookupSine(ang) * 3) >> 4;
  radial[2] = 0;
  sideAng = sideAng + 0x40;
  sideAng = sideAng * 16;
  spacing[0] = LookupCosine(sideAng) >> 5;
  spacing[1] = LookupSine(sideAng) >> 5;
  spacing[2] = 0;

  if (named != 0) {
    if (sign->subMode == NAMESIDE_FRONT) {
      yaw = sign->rotZ - ANGLE8_HALF;
    } else {
      yaw = sign->rotZ;
      ZeroVector(cursor);
      SubtractVector(radial, cursor, radial);
      SubtractVector(spacing, cursor, spacing);
    }
  }

  len = strlen(name);
  n = len - 1;
  ScaleVector3Sat(cursor, spacing, n);
  LShiftVector3(spacing, 1);

  step = n * 2;
  endAng = step & 0xff;
  endAng = endAng * 16;
  cursor[2] = cursor[2] + ((LookupCosine(endAng) * 3) >> 3);
  bend[0] = radial[0] * LookupCosine(endAng);
  bend[1] = radial[1] * LookupCosine(endAng);
  bend[2] = 0;

  i = 0;
  if (len > 0) {
    p = name;
    do {
      int raw = *p;
      int ch = raw & 0xff;
      int arcAng;
      int skin;

      if (ch != ' ') {
        if ((unsigned int)(raw - 'A') < 26) {
          ch = ch + (ACTOR_TYPE_LETTER_A - 'A');
        } else {
          ch = ACTOR_TYPE_GLYPH_COMMA;
        }
        letter = g_pfnLevelSpawnActor(ch, sign);
        REC(letter)->sign = sign;
        REC(letter)->count = len;
        REC(letter)->index = i;

        arcAng = step & 0xff;
        arcAng = arcAng * 16;
        letter->posX = radial[0] * LookupCosine(arcAng);
        letter->posY = radial[1] * LookupCosine(arcAng);
        letter->posZ = 0;

        pos = &letter->posX;
        SubtractVector(pos, pos, bend);
        RShiftVector3(pos, 0xa);
        AddVector(pos, pos, radial);
        AddVector(pos, pos, &sign->posX);
        SubtractVector(pos, pos, cursor);

        pos[2] = pos[2] + ((LookupCosine(arcAng) * 3) >> 3);
        if (letter->type == ACTOR_TYPE_GLYPH_COMMA) {
          pos[2] = pos[2] + 0x100;
        }
        letter->rotZ = yaw;
        letter->floorOffset = yaw & 0xff;
        skin = sign->subMode;
        letter->subMode = i * 8;
        letter->tintMode = TINT_GOLD;
        letter->mode = skin;
      }
      SubtractVector(cursor, cursor, spacing);
      step -= 4;
      i++;
      p++;
    } while (i < len);
  }
}

#undef REC

/* Per-frame tick of a dragon-rescue statue burst shard (0x8003c6e4, 0x178).
   Follows the BurstShardState at ->state: while its life runs and the shard
   is above floorZ, integrates position / velocity (gravity -6 clamped at
   -0x80), spins rotX / rotY / rotZ by the spin bytes and spawns a sparkle
   every other tick; otherwise emits the streak burst and despawns. */
void TickStatueBurstShard(Actor *actor) {
  int burst[3];
  BurstShardState *rec;
  int t;
  int b;

  rec = actor->state;
  if (rec->life != 0 && actor->posZ > rec->floorZ) {
    actor->posX += rec->vel[0];
    actor->posY += rec->vel[1];
    t = rec->vel[2] - 6;
    rec->vel[2] = t;
    if (t < -0x80) {
      rec->vel[2] = -0x80;
    }
    actor->posZ += rec->vel[2];
    actor->rotX += rec->spinX;
    actor->rotY += rec->spinY;
    actor->rotZ += rec->spinZ;
    b = rec->life - 1;
    rec->life = b;
    if ((b & 1) == 0) {
      burst[0] = GetRandomU32() & 3;
      burst[1] = GetRandomU32() & 3;
      burst[2] = 0x14;
      g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, (int)burst);
    }
  } else {
    g_pfnLevelEmitParticles(3, EMIT_STREAK_BURST, &actor->posX, 0x10);
    DespawnActorRecord(actor);
  }
}
