/* actor_type.h — the 0x58-byte actor ("moby") record on its own.
 *
 * actor.h includes this and adds the Actor * API prototypes. Files whose
 * matched source declares those helpers with int * parameters include just
 * this header to view a record through the struct. */

#ifndef OPEN_SPYRO_ACTOR_TYPE_H
#define OPEN_SPYRO_ACTOR_TYPE_H

typedef struct Actor {
  void *state;         /* 0x00 class-specific state block */
  int unk04;           /* 0x04 */
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
  unsigned char mode;  /* 0x48 behaviour state; >= 0x80 dormant, 0xFF ends the pool */
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

/* Byte offset of an Actor field. */
#define ACTOR_OFFSET(field) ((int)&((Actor *)0)->field)

/* An Actor field accessed as a plain byte-offset memory reference rather
 * than a struct member. Same address and width as `actor->field`, but gcc
 * 2.7 does not mark it in-struct, so the scheduler and cse keep it ordered
 * against scalar global stores; a few matched functions need that order.
 * Use it only where `actor->field` was measured to change the output. */
#define ACTOR_RAW(actor, type, field) (*(type *)((char *)(actor) + ACTOR_OFFSET(field)))

#endif /* OPEN_SPYRO_ACTOR_TYPE_H */
