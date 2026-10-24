#include "actor_type.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* Move an actor by a delta with contact/collision checks (0x80039228,
   0x170). In gamestate 3 the delta is scaled by 1.5x (half added), in
   state 4 doubled. flags bit 0 lifts the probe point by 0x12C+radius
   during the checks. Returns 1 on actor contact, 2 on world collision,
   0 when the move was committed. */
extern int DispatchActorContactAtSphere(int *pos, int group, int a, int b,
                                        Actor *actor, int c);
extern void RefreshActorDerivedState(Actor *actor, int mode);

typedef struct {
  int x;
  int y;
  int z;
} DeltaVec;

/* The delta stays a by-value struct: all three words are homed together,
   which the in-place scaling below relies on (three int parameters would
   only home the one whose address is taken). */
int MoveActorByDelta(Actor *actor, DeltaVec delta, int group, int radius,
                     int flags) {
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
  if (flags & 1) {
    t = newpos[2] + 0x12C;
    newpos[2] = t + radius;
  }
  if (group != 0) {
    if (DispatchActorContactAtSphere(newpos, group, 0, 0, actor, 0) != 0) {
      return 1;
    }
  }
  if (radius != 0) {
    if (CollideSphereWithWorldAndActors(newpos, radius, radius) != 0) {
      return 2;
    }
  }
  if (flags & 1) {
    t = newpos[2] - 0x12C;
    newpos[2] = t - radius;
  }
  CopyVector(&actor->posX, newpos);
  RefreshActorDerivedState(actor, 2);
  return 0;
}
