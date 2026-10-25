/* actor_type.h — the 0x58-byte actor ("moby") record on its own.
 *
 * actor.h includes this and adds the Actor * API prototypes. Files whose
 * matched source declares those helpers with int * parameters include just
 * this header to view a record through the struct. */

#ifndef OPEN_SPYRO_ACTOR_TYPE_H
#define OPEN_SPYRO_ACTOR_TYPE_H

typedef struct Actor {
  void *state;         /* 0x00 class-specific state block */
  int pad04;           /* 0x04 */
  int collision;       /* 0x08 collision record; < 0 (a KSEG0 pointer) when the
                        * actor can be hit, 0 / >= 0 when not (overlay target
                        * scans skip those) */
  int posX;            /* 0x0C position */
  int posY;            /* 0x10 */
  int posZ;            /* 0x14 */
  int flags;           /* 0x18 damage flags */
  int groundCode;      /* 0x1C packed ground contact, written by
                        * EncodeCachedVecToActorDirCode from the last ground
                        * probe: bit 31 valid, two 6-bit floor-normal angles
                        * at bits 22 and 16, floor z >> 2 below. Overlays
                        * store -1 / 0 to reset it and toggle bit 31 (bit 31
                        * also puts the actor on BuildActorDrawList's near
                        * list). */
  int mtx[5];          /* 0x20 rotation matrix (packed GTE, see RefreshActorDerivedState) */
  short gridCell;      /* 0x34 spatial-grid cell (RefreshActorDerivedState bit 2) */
  short type;          /* 0x36 actor class */
  short floorOffset;   /* 0x38 signed offset above the ground (SnapActorToGround) */
  unsigned char killGroup; /* 0x3A low 7 bits: kill group (< 0x20 a group bit,
                            * 0x7D/0x7E special; BuildLoadedLevelRuntime);
                            * bit 7 latches the death drop (SpawnActorDeathDrop) */
  unsigned char probeDist; /* 0x3B DriveActorTrackedMove ground-probe distance / 4;
                           * 0 default 0x12C, 0xFF none, 0xFE probe mode 1 */
  unsigned char anim;  /* 0x3C current animation */
  unsigned char nextAnim; /* 0x3D next animation */
  unsigned char frame; /* 0x3E current frame */
  unsigned char nextFrame; /* 0x3F next frame */
  unsigned char frameProgress; /* 0x40 frame progress */
  unsigned char frameRate; /* 0x41 progress per frame */
  unsigned char animFlags; /* 0x42 bit 1 done (g_nActorAnimDone), bit 0 new frame */
  unsigned char group; /* 0x43 activation group (BuildActiveActorList) */
  unsigned char rotX;  /* 0x44 euler x (8-bit angle) */
  unsigned char rotY;  /* 0x45 euler y */
  unsigned char rotZ;  /* 0x46 euler z (yaw) */
  unsigned char otBias; /* 0x47 signed OT-bin bias: RasterizeSpritePrimQueue
                        * bins at (MAC3 >> 6) - otBias, clamped >= 0. Default 4
                        * (InitActorRenderDefaults), 0x20 HUD/screen actors,
                        * 0x7F sprite records, 0xFE (-2) pushes it one bin pair deeper */
  unsigned char mode;  /* 0x48 behaviour state; >= ACTOR_MODE_DORMANT dormant,
                          ACTOR_MODE_END ends the pool */
  unsigned char subMode; /* 0x49 sub-state */
  unsigned char visZone; /* 0x4A visibility-gate index into g_abVisZoneGates;
                          * 0xFF = always drawn (BuildRenderEntityLists) */
  unsigned char renderList; /* 0x4B sign picks the BuildRenderEntityLists
                             * bucket: 0, < 0, > 0 (default 0x20) */
  /* 0x4C..0x4F tint. Read as one word, 0x4C..0x4E pack three 6-bit levels
   * under a 0xA tag: 0xA << 20 | c2 << 12 | c1 << 6 | c0. SelectActorLodBand
   * builds it with 0x18 as neutral, which gives the 0xA18618 the overlays
   * store, and the flame band raises c2 while lowering c0/c1. With 0x4C..0x4E
   * zeroed, tintMode picks a preset instead: gem colours 1..5 (HUD icons set
   * type - 0x52) and 0xB for HUD sprites. TickActorFlameGlow drives tint2 as a
   * 0..0x60 glow on its own. These names describe the byte layout only; the
   * renderer that reads them is hand-written asm and not decoded yet. */
  unsigned char tint0; /* 0x4C */
  unsigned char tint1; /* 0x4D */
  unsigned char tint2; /* 0x4E */
  unsigned char tintMode; /* 0x4F */
  unsigned char cullRadius; /* 0x50 view-space cull bound >> 8 (BuildActorDrawList
                            * loads it as the halfword with ->drawn) */
  unsigned char drawn; /* 0x51 drawn last frame: always kept active */
  unsigned char updateDist; /* 0x52 activation radius >> 10 */
  unsigned char dropKind; /* 0x53 death-drop item id (& 0x7F, 1..0x7E fixed;
                           * 0xFF = random gem roll) — SpawnActorDeathDrop,
                           * IsActorInRunningKillSet */
  unsigned char sfxSlot; /* 0x54 sound owner key (PlaySoundEffect) */
  unsigned char soundRange; /* 0x55 voice audible range >> 10 (TickActiveSoundVoices) */
  unsigned char poolIndex; /* 0x56 id byte for actors outside the pool */
  unsigned char scale; /* 0x57 draw scale override, 0 = mesh default (likely; overlay
                        * grow/shrink tweens drive it 0x20..0x60, BuildActorDrawList
                        * packs it into the draw word) */
} Actor; /* 0x58 */

/* Size of one actor record: the stride of the actor pool, the sprite-record
 * queue and the HUD icon table. (The typedef fails to compile if the struct
 * ever drifts from it; it emits nothing.) */
/* Actor.mode sentinels: UpdateActors and the scans skip dormant records, and
 * the pool ends at the first record whose mode is 0xFF. */
#define ACTOR_MODE_DORMANT 0x80
#define ACTOR_MODE_END 0xFF

#define ACTOR_SIZE 0x58
typedef char ActorSizeCheck[sizeof(Actor) == ACTOR_SIZE ? 1 : -1];

/* Byte offset of an Actor field. */
#define ACTOR_OFFSET(field) ((int)&((Actor *)0)->field)

/* An Actor field accessed as a plain byte-offset memory reference rather
 * than a struct member. Same address and width as `actor->field`, but gcc
 * 2.7 does not mark it in-struct, so the scheduler and cse keep it ordered
 * against scalar global stores; a few matched functions need that order.
 * Use it only where `actor->field` was measured to change the output. */
#define ACTOR_RAW(actor, type, field) (*(type *)((char *)(actor) + ACTOR_OFFSET(field)))

/* Actor->type values that mean the same class in every level (one global
 * g_apActorMeshTable index space). Only classes whose behaviour is pinned
 * down are named; the rest stay numeric in the per-level switches. */
enum ActorType {
  ACTOR_TYPE_LEVEL_EXIT = 9,       /* exit whirlwind: lifts Spyro back to the hub */
  ACTOR_TYPE_EXTRA_LIFE = 14,      /* collected: g_nExtraLives + 1 */
  ACTOR_TYPE_SPARX_FOOD = 15,      /* collected: g_nSparxFoodCount + 1 (20: a life) */
  ACTOR_TYPE_BUTTERFLY = 16,       /* eaten by Sparx: g_nSparxHealth + 1 */
  ACTOR_TYPE_DRAGON_EGG = 34,      /* recovered egg: rises, rolls the HUD egg tally */
  ACTOR_TYPE_LETTER_OTHER = 0x4C,  /* a non-A..Z glyph of a floating name */
  ACTOR_TYPE_GEM_1 = 0x53,         /* gems, by CollectPickupActor's value */
  ACTOR_TYPE_GEM_2 = 0x54,
  ACTOR_TYPE_GEM_5 = 0x55,
  ACTOR_TYPE_GEM_10 = 0x56,
  ACTOR_TYPE_GEM_25 = 0x57,
  ACTOR_TYPE_SAVE_HOST = 110,      /* hovers near its anchor, opens the save menu (SaveHostState) */
  ACTOR_TYPE_SPARX = 0x78,         /* g_pSparxActor (SparxState) */
  ACTOR_TYPE_KEY = 173,            /* the level key: g_nLevelKeyState = 1 */
  ACTOR_TYPE_LOCKED_CHEST = 174,   /* opened with the key, drops a reward */
  ACTOR_TYPE_DRAGON_STATUE = 0xFA, /* trapped dragon: counts the rescue */
  ACTOR_TYPE_STATUE_SHARD = 0xFB,  /* its burst shards (BurstShardState) */
  ACTOR_TYPE_GEM_DIGIT_0 = 0x104,  /* floating gem-value digits 0..9: 0x104..0x10D */
  ACTOR_TYPE_PORTAL_GUIDE = 398,  /* flies Spyro through a portal (PortalState) */
  ACTOR_TYPE_LETTER_A = 0x1AA,     /* floating-name letters A..Z: 0x1AA..0x1C3 */
};

/* Waypoint path used by the path-following classes (10, 114, 339) and
 * FollowActorWaypointPath / PivotActorAlongPath: node count, current node,
 * direction, then 0x10-byte nodes that begin with a position. */
typedef struct PathData {
  unsigned char count; /* 0x00 */
  unsigned char cur;   /* 0x01 */
  char pad02[4];
  short reversed;      /* 0x06 -1: walk forward (wrap to 0), else backward */
  struct {
    int x;
    int y;
    int z;
    int speed; /* > 0 overrides the follow speed at this node */
  } nodes[1];          /* 0x08 */
} PathData;

/* UpdateActorWander's behaviour block (the head of the wandering classes'
 * state, e.g. level 32 type 236). Radii are stored >> 10. */
typedef struct WanderBlock {
  int homeX;                     /* 0x00 home point */
  int homeY;                     /* 0x04 */
  int pad08;                     /* 0x08 */
  unsigned char speed;           /* 0x0C move speed (x1.5 / x2 at frame delta 3 / 4) */
  unsigned char turnRate;        /* 0x0D */
  unsigned char contact;         /* 0x0E move contact group, also the probe radius */
  unsigned char repickMin;       /* 0x0F re-pick countdown range */
  unsigned char repickMax;       /* 0x10 */
  unsigned char jitterMin;       /* 0x11 heading jitter range */
  unsigned char jitterMax;       /* 0x12 */
  unsigned char leash;           /* 0x13 walk home once farther than this */
  unsigned char weaveAmp;        /* 0x14 0 = no weave */
  unsigned char weaveRate;       /* 0x15 */
  unsigned char pad16;           /* 0x16 */
  unsigned char repickTimer;     /* 0x17 frames to the next heading jitter */
  unsigned char heading;         /* 0x18 8-bit angle */
  unsigned char weaveDir;        /* 0x19 +1 / -1 */
  unsigned char chaseRadius;     /* 0x1A 0 = only the fixed 0x44C range */
  unsigned char chasing;         /* 0x1B */
  unsigned char chaseCooldown;   /* 0x1C set after a wall bump or a walk home */
  unsigned char contactCooldown; /* 0x1D moves ignore the contact group while set */
  short weave;                   /* 0x1E current weave offset */
} WanderBlock; /* 0x20 */

#endif /* OPEN_SPYRO_ACTOR_TYPE_H */
