/* level_update_actors_macros.h — helper macros the *_UpdateActors.c clones
 * spelled identically, defined once. They expand at the use site, so they
 * may name the function's locals (actor, st, ...) and file-scope helpers.
 * Included after level_update_actors.h. */

#ifndef OPEN_SPYRO_LEVEL_UPDATE_ACTORS_MACROS_H
#define OPEN_SPYRO_LEVEL_UPDATE_ACTORS_MACROS_H

/* Node accessors for paths held as byte pointers (type 286). */
#define PATH_X(p, i) (*(int *)((p) + ((i) << 4) + 0x8))

#define PATH_Y(p, i) (*(int *)((p) + ((i) << 4) + 0xC))

#define PATH_Z(p, i) (*(int *)((p) + ((i) << 4) + 0x10))

/* Progress-per-tick byte of animation `anim` in the actor's model. */
#define TYPE_DESC(a, id)                                                     \
  (g_apActorMeshTable[(a)->type]->m_Animations[(id)]->m_ProgressPerTick)

#define AS_I32(sym) (((I32 *)&(sym))->v)

#define AS_PTRU8(sym) (((PtrU8 *)&(sym))->v)

#define LIFT_STEP(st) (((st)->phase - 0x1A0) >> 1)

#define CAM g_anWorldToCameraRotMtx

#define ABS(x) ((x) >= 0 ? (x) : -(x))

#define ABS2(x) ((x) > 0 ? (x) : -(x))

/* Animation state: current/next animation (0x3C/0x3D), current/next frame
 * (0x3E/0x3F), frame progress (0x40) and progress per frame (0x41).
 *
 * ANIM_RESET restarts `anim` from frame 0 at the model's own speed. */
#define ANIM_RESET(a, id)                                                    \
  (a)->frameProgress = 0;                                                              \
  (a)->frameRate = TYPE_DESC(a, id);                                             \
  (a)->anim = (id);                                                         \
  (a)->nextAnim = (id);                                                         \
  (a)->frame = 0;                                                              \
  (a)->nextFrame = 1;

/* The request velocity's Y word, read through the vector view. */
#define g_nSpyroRequestVelocityY g_anSpyroRequestVelocity.y

/* An animation change that also clears the finished flag. */
#define ANIM_GO(a, n)                                                          \
  if ((a)->nextAnim != (n)) {                                                     \
    g_nActorAnimDone = 0;                                                            \
    ANIM_ADVANCE(a, n);                                                        \
  }

#define ANIM_SET(a, n)                                                         \
  if ((a)->anim != (n)) {                                                     \
    g_nActorAnimDone = 0;                                                            \
    ANIM_RESET(a, n);                                                          \
  }

/* ENTER_POSE with the already-there test first. */
#define ENTER_POSE4(a, n)                                                      \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim == (n)) {                                                     \
    continue;                                                                  \
  }                                                                            \
  ANIM_ADVANCE(a, n);                                                          \
  continue

/* An animation change at half speed, keyed on the current animation. */
#define ANIM_BLEND(a, n)                                                       \
  if ((a)->anim != (n)) {                                                     \
    (a)->frameProgress = 8;                                                            \
    (a)->frameRate = 8;                                                            \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    OnActorAnimQueued(a);                                                          \
  }

/* ENTER_POSE with an empty loop at the head of the change: cse starts a new
 * block there, so the `nextAnim = n` store takes its own copy of the constant
 * (merged again after loop.c). Keeps the constant's lifetime short enough
 * that loop.c does not hoist it (see case 115). */
#define ENTER_POSE_B(a, n)                                                     \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim != (n)) {                                                     \
    do {                                                                       \
    } while (0);                                                               \
    (a)->frameProgress = sixteen;                                                      \
    (a)->frameRate = sixteen;                                                      \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    OnActorAnimQueued(a);                                                          \
  }                                                                            \
  continue

#define CAM_LOOK (*(CamLook *)&g_anSpyroRequestVelocity)

/* Case 402 -- cannon ball: fired from a barrel, then flown along a path
 * whose header is { count, cur } and whose points follow at +8 (PATH_X). */
#define PATH_PT(p, i) ((int *)((p) + (((i) << 4) + 0x8)))

/* The anim bytes 0x3C..0x3F read as one word. */
#define ANIM_WORD(a) (*(int *)&(a)->anim)

/* Spyro's facing delta to an actor. */
#define SPY_FACING(a)                                                          \
  AbsAngleDelta8(spy->bodyRotZ, ArcTan2((a)->posX - spy->posX,            \
                                             (a)->posY - spy->posY, 0))

/* An actor's height above Spyro. */
#define SPY_DZ(a) ((a)->posZ - (a)->floorOffset - SPY.posZ)

/* Advance to animation `n` with frame progress 8 (a slower blend). */
#define ANIM_GO8(a, n)                                                         \
  if ((a)->nextAnim != (n)) {                                                     \
    g_nActorAnimDone = 0;                                                            \
    (a)->frameProgress = 8;                                                            \
    (a)->frameRate = 8;                                                            \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    OnActorAnimQueued(a);                                                          \
  }

/* Re-time the current animation once its frames are within 3 of each
 * other. */
#define ANIM_RETIME(a)                                                         \
  if (ABS2((a)->nextFrame - (a)->frame) < 3) {                                     \
    (a)->frameRate = TYPE_DESC(a, (a)->anim);                                     \
  }

/* Jump the queued animation to frame `f` of animation `n`. */
#define ANIM_JUMP(a, n, f)                                                     \
  g_nActorAnimDone = 0;                                                              \
  (a)->frameProgress = sixteen;                                                        \
  (a)->frameRate = sixteen;                                                        \
  (a)->anim = (a)->nextAnim;                                                     \
  (a)->nextAnim = (n);                                                            \
  (a)->frame = (a)->nextFrame;                                                     \
  (a)->nextFrame = (f);                                                            \
  OnActorAnimQueued(a)

/* Set the state byte and queue animation `n` without promoting the current
 * one, then move on to the next actor. */
#define ANIM_QUEUE(a, n)                                                       \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim != (n)) {                                                     \
    (a)->frameProgress = sixteen;                                                      \
    (a)->frameRate = sixteen;                                                      \
    (a)->nextAnim = (n);                                                          \
    (a)->nextFrame = 0;                                                            \
    OnActorAnimQueued(a);                                                          \
  }                                                                            \
  continue

/* Clear the partner's "held" bit. */
#define L25_UNLINK(st)                                                         \
  if ((st)->link >= 0) {                                                       \
    g_pActorListBase[(st)->link].subMode &= 0xFB;                                      \
  }

/* Give up the shared slot. */
#define L25_DROP(st)                                                           \
  *(st)->slot = 0;                                                             \
  L25_UNLINK(st)

#define AS_PTRV(sym) (((PtrV *)&(sym))->v)

/* TurnActorYawTowardAngle called with its first four arguments only. */
#define FACE4(a, h, s, t)                                                      \
  ((int (*)(Actor *, int, int, int))TurnActorYawTowardAngle)(a, h, s, t)

/* Back to the idle state, restarting animation 0. */
#define RESET_IDLE(a)                                                          \
  (a)->mode = 0;                                                              \
  ANIM_RESET(a, 0);                                                            \
  continue

/* Burst a barrel: splash, release its rider, and remove it. */
#define BARREL_BURST(a, st)                                                    \
  {                                                                            \
    Actor *sp_ = g_pfnLevelSpawnActor(ACTOR_TYPE_SPLASH, a);                                         \
    Actor *w_;                                                                 \
                                                                               \
    StopSoundVoicesByOwner(a, VOICE_STOP_ALL);                                                       \
    PlaySoundEffect(g_apActorMeshTable[(a)->type]->m_Sounds[3], sp_, SFX_MODE_3D, &sp_->voiceRow);    \
    w_ = (st)->rider;                                                          \
    if (w_ != 0 && w_->type == ACTOR_TYPE_HATCH && w_->mode == HATCH_OPEN) {                   \
      w_->subMode = 1;                                                           \
    }                                                                          \
    DespawnActorRecord(a);                                                          \
    continue;                                                                  \
  }

#define FLY_Z(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x10))

#define FP_X(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x8))

#define FP_Y(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0xC))

#define FP_PT(p, i) ((int *)((char *)(p) + ((i) << 4) + 0x8))

#define FLY_X(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x8))

#define FLY_Y(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0xC))

#define RUN_FLAGS(p) (*(int *)((p) + ((p)[1] << 4) + 0x14))

#define ROCK(st) (*(Actor **)((char *)(st) + 0x40))

/* An animation change that keeps the current animation (no 3C/3E promotion). */
#define ANIM_THEN(a, n)                                                        \
  if ((a)->nextAnim != (n)) {                                                     \
    g_nActorAnimDone = 0;                                                            \
    (a)->frameProgress = sixteen;                                                      \
    (a)->frameRate = sixteen;                                                      \
    (a)->nextAnim = (n);                                                          \
    (a)->nextFrame = 0;                                                            \
    OnActorAnimQueued(a);                                                          \
  }

#endif /* OPEN_SPYRO_LEVEL_UPDATE_ACTORS_MACROS_H */
