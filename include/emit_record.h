/* emit_record.h — the 0x20-byte emit-list record (g_pEmitListBase) as the
 * EmitParticles clones build it: a common head, then one layout per record
 * class. The union is the superset of every clone's members; a level only
 * touches the classes it emits. AdvanceEmitList and RasterizeEmitList read
 * the same records through byte offsets (level_advance_emit_list.h). */

#ifndef OPEN_SPYRO_EMIT_RECORD_H
#define OPEN_SPYRO_EMIT_RECORD_H

typedef struct Emit {   /* one 0x20-byte emit-list record */
  unsigned char type;   /* 0x00 effect id */
  unsigned char cls;    /* 0x01 record class (stamped by the allocator) */
  unsigned char phase;  /* 0x02 */
  unsigned char visible;  /* 0x03 1 = drawn last frame (RasterizeEmitList); AdvanceEmitList frees it at 0 */
  union {
    struct {                     /* classes 2 / 3 / 6 -- spark */
      short pos[3];              /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      unsigned char texIndex;       /* 0x10 sub-record (texture) index */
      unsigned char depthBias;       /* 0x11 OT depth-bin bias */
      short pad[3];              /* 0x12 */
      short vel[3];              /* 0x18 */
      short toggle;               /* 0x1E phase toggle (AdvanceEmitList flips it 0<->1) */
    } spark;
    struct {                     /* class 0 -- dust mote */
      unsigned short pos[3];     /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      short vel[3];              /* 0x10 */
      short pad16;               /* 0x16 */
      int src;                   /* 0x18 the emitter argument it tracks */
      int waypoint;                 /* 0x1C index into the waypoint list at src (stream-follower 0x45) */
    } dust;
    struct {                     /* class 2 -- ribbon (2nd point at 0x12) */
      short pos[3];              /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      unsigned char texIndex;       /* 0x10 sub-record (texture) index */
      unsigned char depthBias;       /* 0x11 OT depth-bin bias */
      short pos2[3];             /* 0x12 */
      unsigned char angle;       /* 0x18 swirl angle (LUT index, += step*4) */
      unsigned char radius;       /* 0x19 swirl radius (grows by step; freed past 0xC0) */
    } ribbon;
    struct {                     /* class 2 -- band (shorts at 0x18 / 0x1A) */
      short pos[3];              /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      unsigned char texIndex;       /* 0x10 sub-record (texture) index */
      unsigned char depthBias;       /* 0x11 OT depth-bin bias */
      short pos2[3];             /* 0x12 */
      short lift;               /* 0x18 fire-jet height lift (scaled by segment and heat) */
      short radius;               /* 0x1A fire-jet ring radius */
      unsigned char angle;       /* 0x1C fire-jet spin angle (LUT index, += step*8) */
      unsigned char segment;       /* 0x1D fire-jet segment index */
    } band;
    struct {                     /* class 0 -- smear (2nd point at 0x10) */
      short pos[3];              /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      short pos2[3];             /* 0x10 */
      short pad16;               /* 0x16 */
      unsigned char angle;       /* 0x18 swirl angle (LUT index, += step*4) */
      unsigned char radius;       /* 0x19 swirl radius (grows by step; freed past 0xC0) */
    } smear;
    struct {                     /* class 3 -- actor-tracking plume */
      unsigned short pos[3];     /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      unsigned char texIndex;       /* 0x10 sub-record (texture) index */
      unsigned char depthBias;       /* 0x11 OT depth-bin bias */
      short pad[3];              /* 0x12 */
      int *src;                  /* 0x18 the actor position it tracks */
      short maxRadius;               /* 0x1C orbit radius cap (orbiting spark 0x6) */
      short rise;               /* 0x1E rise limit in 0x40 units (orbiting spark 0x6); zeroed elsewhere */
    } plume;
    struct {                     /* class 3 -- owner-tracked spark */
      unsigned short pos[3];     /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      unsigned char texIndex;       /* 0x10 sub-record (texture) index */
      unsigned char depthBias;       /* 0x11 OT depth-bin bias */
      short pad[3];              /* 0x12 */
      short vel[3];              /* 0x18 */
      unsigned char owner;       /* 0x1E actor index into the pool */
      unsigned char orbitRadius;       /* 0x1F orbit radius (two-axis orbit 0x49) */
    } track;
    struct {                     /* classes 1 / 4 -- streak */
      unsigned short a[3];       /* 0x04 head */
      unsigned short b[3];       /* 0x0A tail */
      unsigned char c0[4];       /* 0x10 head colour */
      unsigned char c1[4];       /* 0x14 tail colour */
    } line;
    struct {                     /* class 3 -- owner-tethered spark */
      unsigned short pos[3];     /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      unsigned char texIndex;       /* 0x10 sub-record (texture) index */
      unsigned char depthBias;       /* 0x11 OT depth-bin bias */
      short pad[3];              /* 0x12 */
      unsigned char owner;       /* 0x18 actor index into the pool */
      unsigned char node;        /* 0x19 patrol node index (ghost 0x1D) */
      unsigned char waypoint;    /* 0x1A waypoint index (ghost 0x1D) */
      unsigned char pad1B;       /* 0x1B */
      short pad1C;               /* 0x1C */
      short flags;               /* 0x1E sub-state bits: 8 trail, 1 orbit, 0 patrol */
    } tether;
    struct {                     /* class 2 -- haze puff (word velocity) */
      unsigned short pos[3];     /* 0x04 */
      unsigned char life;        /* 0x0A */
      unsigned char seed;        /* 0x0B */
      unsigned char r;           /* 0x0C */
      unsigned char g;           /* 0x0D */
      unsigned char b;           /* 0x0E */
      unsigned char fx;          /* 0x0F */
      unsigned char texIndex;       /* 0x10 sub-record (texture) index */
      unsigned char depthBias;       /* 0x11 OT depth-bin bias */
      short pad[3];              /* 0x12 */
      int vel;                   /* 0x18 */
    } puff;
  } u;
} Emit;

/* Effect types for g_pfnLevelEmitParticles(count, type, pos, effectArg)
 * whose arm is the same in all 35 level EmitParticles clones. The rest stay
 * numeric. */
enum EmitEffect {
  EMIT_SPARK_VEL = 0x0,     /* spark moving at the velocity effectArg points to, 0x18 frames
                             * (the flame breath's particles) */
  EMIT_SPARK_VEL_DIM = 0x1, /* dimmer, shorter EMIT_SPARK_VEL (8 frames during the dragon rescue;
                             * the superflame's particles) */
  EMIT_SPARK_RISE = 0x2,    /* spark drifting up with a random XY jitter */
  EMIT_ACTOR_SPARK = 0xC,   /* spark at an emit offset of the Actor * in pos; effectArg packed RGB
                             * (bits 24+ pick the offset slot for keys) */
  EMIT_TWINKLE = 0x42,      /* 2-frame twinkle: yellow for effectArg 1 (or 0 while Sparx is at full
                             * health), else a random colour */
  EMIT_STREAK_BURST = 0x46, /* yellow-orange streaks thrown out to radius effectArg */
  EMIT_STREAK_BURST_WHITE = 0x47, /* small white-yellow streak burst */
};

#endif /* OPEN_SPYRO_EMIT_RECORD_H */
