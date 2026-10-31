#include "actor.h"
#include "globals.h"
#include "vecmath.h"

/* OR `bits` into the +0x18 flags word of every actor in pickup group `group`
   that lies inside a box + cone around actor `actor` (0x8003b294): per-axis
   |dx|,|dy| < xyLim, ApproxDist2D < distLim, |dz| < zLim, and the 8-bit
   heading from `actor` to the member within angLim of actor's facing byte
   (+0x46). Dormant members (mode >= ACTOR_MODE_DORMANT) are skipped. */

void OrActorGroupFlagsInCone(Actor *actor, int group, unsigned int bits,
                             int distLim, int zLim, int xyLim, int angLim) {
  unsigned short *ptr;
  int idx;
  Actor *rec;
  int d;

  if (group < g_nLevelPickupTableCount) {
    ptr = ((unsigned short **)g_pLevelPickupTableEntries)[group];
    do {
      idx = *ptr & 0x7FFF;
      rec = &((Actor *)g_pActorListBase)[idx];
      if (rec->mode < ACTOR_MODE_DORMANT) {
        d = actor->posX - rec->posX;
        if (d > 0) {
          if (d >= xyLim) {
            continue;
          }
          goto xdone;
        }
        if (rec->posX - actor->posX < xyLim) {
          goto xdone;
        }
        continue;
      xdone:
        d = actor->posY - rec->posY;
        if (d > 0) {
          if (d < xyLim) {
            goto ydone;
          }
          continue;
        }
        if (rec->posY - actor->posY >= xyLim) {
          continue;
        }
      ydone:
        if (ApproxDist2D(&actor->posX, &rec->posX) >= distLim) {
          continue;
        }
        d = rec->posZ - actor->posZ;
        if (d > 0) {
          if (d < zLim) {
            goto zdone;
          }
          continue;
        }
        if (actor->posZ - rec->posZ >= zLim) {
          continue;
        }
      zdone:
        if (AbsAngleDelta8(actor->rotZ, ArcTan2(rec->posX - actor->posX,
                                                rec->posY - actor->posY, 0)) <
            angLim) {
          rec->flags |= bits;
        }
      }
    } while ((short)*ptr++ >= 0);
  }
}
