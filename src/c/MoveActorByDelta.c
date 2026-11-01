#include "actor_type.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* Move an actor by a delta with contact/collision checks (0x80039228,
   0x170). At frame delta 3 the delta is scaled by 1.5x (half added), at
   4 doubled. Actors inside the `contactRadius` sphere get their contact
   handlers (0 skips the test); MOVE_LIFT lifts the probe point by
   0x12C+radius during the world-collision check. Returns 1 on actor contact, 2
   on world collision, 0 when the move was committed. */
/* Local prototypes, not funcs.h: its caller-side MoveActorByDelta splits the
   DeltaVec into three ints and conflicts with the definition below. */
#ifndef CONTACT_STOP_AT_SOLID
#define CONTACT_STOP_AT_SOLID                                                  \
  0 /* stop at the first solid shape; returns the last actor touched */
#define CONTACT_PUSH_OUT                                                       \
  1 /* push pos out of the first solid shape (written back); returns that      \
       actor */
#define CONTACT_ALL 2 /* keep testing past solid shapes */
#endif
extern int DispatchActorContactAtSphere(int *pos, int radius, int events,
                                        int hitBits, Actor *self, int mode);
extern void RefreshActorDerivedState(Actor *actor, int mode);

typedef struct {
  int x;
  int y;
  int z;
} DeltaVec;

/* The delta stays a by-value struct: all three words are homed together,
   which the in-place scaling below relies on (three int parameters would
   only home the one whose address is taken). */
int MoveActorByDelta(Actor *actor, DeltaVec delta, int contactRadius,
                     int radius, int flags) {
  int newpos[4];
  int half[4];
  int *h;
  int *d;
  int t;

  if (g_nActorFrameDelta == 3) {
    h = half;
    d = (int *)&delta;
    CopyVector(h, d);
    RShiftVector3(h, 1);
    AddVector(d, d, h);
  } else if (g_nActorFrameDelta == 4) {
    LShiftVector3((int *)&delta, 1);
  }
  AddVector(newpos, &actor->posX, (int *)&delta);
  if (flags & MOVE_LIFT) {
    t = newpos[2] + 0x12C;
    newpos[2] = t + radius;
  }
  if (contactRadius != 0) {
    if (DispatchActorContactAtSphere(newpos, contactRadius, 0, 0, actor,
                                     CONTACT_STOP_AT_SOLID) != 0) {
      return 1;
    }
  }
  if (radius != 0) {
    if (CollideSphereWithWorldAndActors(newpos, radius, radius) != 0) {
      return 2;
    }
  }
  if (flags & MOVE_LIFT) {
    t = newpos[2] - 0x12C;
    newpos[2] = t - radius;
  }
  CopyVector(&actor->posX, newpos);
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  return 0;
}
