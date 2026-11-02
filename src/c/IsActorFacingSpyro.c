#include "actor_type.h"
#include "globals.h"
#include "vecmath.h"

/* True when the actor's byte yaw (rotZ) points within `threshold` of the
   direction from the actor to Spyro. */
int IsActorFacingSpyro(Actor *actor, int threshold) {
  return AbsAngleDelta8(ArcTan2(g_anSpyroWorldPos[0] - actor->posX,
                                g_anSpyroWorldPos[1] - actor->posY,
                                ATAN_ANGLE8),
                        actor->rotZ) < threshold;
}
