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

/* Record classes (Emit.cls): AllocEmitListRecord stamps one, and
 * RasterizeEmitList picks the primitive by it. */
enum EmitClass {
  EMIT_CLASS_DOT = 0,      /* 1-pixel flat line (LINE_F2) at pos */
  EMIT_CLASS_LINE = 1,     /* two-colour line between two endpoints */
  EMIT_CLASS_LIT_QUAD = 2, /* lit spinning billboard (POLY_FT4) */
  EMIT_CLASS_ALT_LIT_QUAD = 3, /* billboard with the alternate lighting */
  EMIT_CLASS_LINE_ABS = 4, /* EMIT_CLASS_LINE with absolute positions */
  EMIT_CLASS_FADED_QUAD = 5, /* depth-faded spinning billboard */
  EMIT_CLASS_QUAD = 6      /* plain spinning billboard */
};

/* GPU command codes for the `fx` byte: it is the top byte of the colour
 * word at +0x0C, which RasterizeEmitList copies into the primitive. */
#define EMIT_FX_LINE 0x40       /* flat line */
#define EMIT_FX_QUAD 0x2C       /* textured quad */
#define EMIT_FX_QUAD_BLEND 0x2E /* textured quad, semi-transparent */

/* Effect types for g_pfnLevelEmitParticles(count, type, pos, effectArg)
 * whose arm is the same in every level EmitParticles clone that has it.
 * 0xB, 0x45, 0x48 and 0x49 have only AdvanceEmitList arms (no level
 * EmitParticles creates them) and stay numeric. */
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

  /* Named from the EmitParticles arm (what it writes) and the matching
   * AdvanceEmitList arm (how it moves). Where the clones differ, they differ
   * only in statement order. */
  EMIT_ACTOR_PLUME = 0x6,    /* class-3 plume jittered around the Actor * in pos;
                              * radius and rise come from its state words 1 and 0;
                              * random colour when its subMode is set */
  EMIT_SPEED_DUST = 0x9,     /* Spyro's run dust: a spark at the velocity effectArg
                              * points to, red/orange/yellow/grey by g_nSpyroMotionSpeed */
  EMIT_SPYRO_ORBIT_SPARKLE = 0xA, /* sparkle circling Spyro; freed when he is hidden */
  EMIT_FADING_SPARK = 0x10,  /* spark from pos[0..2] at velocity pos[3..5]; effectArg ->
                              * {life, grey >> 3}; all channels fade by 4 a frame */
  EMIT_SPARK_RGB = 0x11,     /* spark from pos[0..2] at velocity pos[3..5]; effectArg ->
                              * {r, g, b}; red cut off after frame 9, dies at 0xC */
  EMIT_RISING_SPIRAL = 0x15, /* point that spirals out and up from pos, effectArg
                              * picks the start angle and tint; freed at radius 0xC1 */
  EMIT_RISING_SPIRAL_SMEAR = 0x4C, /* EMIT_RISING_SPIRAL as a class-0 smear */
  EMIT_DRIFT_STREAK = 0x4D,  /* pink streak from a random point near pos, closing on
                              * its tail while drifting +y; 0x20 frames */
  EMIT_EMBER = 0x4E,         /* warm ember jittered around pos that wanders and fades;
                              * 0x28 frames */
  EMIT_FLAME_STREAK = 0x4F,  /* flame streak from pos toward pos + *effectArg, coloured
                              * by g_nSpyroFlameVariant; 0x18 frames */
  EMIT_DRIFT_SPARKLE = 0x7,  /* grey sparkle jittered within effectArg of pos, drifting at random;
                              * brightness ramps up then down over 0x19 frames */
  EMIT_TRACKING_SPARK = 0x8, /* spark from an attach point of the Actor * in pos, tied to the
                              * Actor * effectArg (track record) */
  EMIT_SPARK_YELLOW = 0xD,   /* yellow spark at the velocity effectArg points to */
  EMIT_GEM_COLOR_BURST = 0x40, /* callers pass a gem colour (prize type - 0x53), but no level's
                                * EmitParticles has an arm for it: the call emits nothing */
  EMIT_ACTOR_WHITE_STREAK = 0x41, /* short white streak jittered over the Actor * in pos; 8 frames */
  EMIT_YELLOW_DUST = 0x4A,   /* class-0 yellow dust at pos moving at the velocity effectArg points to */
  EMIT_FALLING_STREAK_BURST = 0x50, /* EMIT_STREAK_BURST whose streaks keep their speed
                                     * and fall instead of easing out */

  /* Named from the arm pair as above; most live in only a few levels. */
  EMIT_HOMING_ORB = 0x4,     /* class-3 orb jittered around pos that homes on the Actor *
                              * effectArg, orbits, or trails (flag halfword +0x1E) */
  EMIT_JITTER_DRIFT = 0x5,   /* class-3 point jittered within 0x1F of pos that wanders and
                              * sinks; 0x30 frames */
  EMIT_SLOWING_SPARK = 0xE,  /* grey spark at the velocity effectArg points to, slowing in x/y and
                              * fading with camera distance; 0x80 frames */
  EMIT_JITTER_BURST = 0xF,   /* EMIT_JITTER_DRIFT thrown out at a random velocity */
  EMIT_SMOKE_PUFF = 0x13,    /* grey puff that rises, slows, grows and fades with camera
                              * distance; 0x80 frames */
  EMIT_DRIFTING_MOTE = 0x16, /* blue mote within 300 of pos drifting along heading effectArg,
                              * slowing and fading; 0x40 frames */
  EMIT_SCATTER_MOTE = 0x17,  /* grey mote thrown out at a random velocity; 0x30 frames */
  EMIT_RESCUE_SWIRL = 0x18,  /* the dragon rescue's departing swirl: `count` segments circling
                              * pos, radius and height driven by g_nDragonRescueTimer */
  EMIT_HEAT_SPARK = 0x19,    /* spark from pos[0..2] at velocity pos[3..5], grey / yellow /
                              * orange / red as pos[6] passes 321 / 401 / 461; 0x20 frames */
  EMIT_FLIGHT_TRAIL = 0x1A,  /* flight trail spark left at Spyro's two trail points, grey
                              * effectArg * 8, fading 8 a frame */
  EMIT_FLIGHT_GROUND_WAKE = 0x1B, /* flight wake under Spyro when he skims a trigger region
                                   * within 0xC00 of the ground; effectArg = {life, grey} */
  EMIT_ACTOR_RING_SPARK = 0x1C, /* spark flung out at radius effectArg in the YZ plane of the
                                 * Actor * in pos; 0x20 frames */
  EMIT_PATH_WISP = 0x1D,     /* green wisp that walks the waypoint list of the Actor * effectArg
                              * and wakes the last node's actor */
  EMIT_RISING_PUFF = 0x1E,   /* grey puff that rises, grows and fades; 0x20 frames */
  EMIT_SPREAD_MOTE = 0x1F,   /* mote thrown out on a random heading, rising; 0x40 frames */
  EMIT_DUST_PUFF = 0x21,     /* Spyro's dust puff (landing, skid, turn, bonk) at his feet, or
                              * 0x164 ahead of his body for effectArg 1, moving at pos */
};

#endif /* OPEN_SPYRO_EMIT_RECORD_H */
