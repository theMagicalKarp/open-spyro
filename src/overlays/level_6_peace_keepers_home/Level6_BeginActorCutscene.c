/* Level6_BeginActorCutscene (0x8007B878, level_6_peace_keepers_home overlay).
 *
 * Begin the level's gem/dragon cutscene focused on `actor`: enter gamestate
 * 0xC, clear the cutscene timers (g_nGamestate0cSubstate..F0), record the focus actor
 * (g_pGamestate0cActor) and its path end (g_pGamestate0cRecord = actor pool + count*0x58),
 * then aim the cutscene camera: yaw from the XY delta between the camera
 * anchor (g_anCameraPos..) and the actor position (arctan via ArcTan2),
 * pitch/roll from the transformed offset (SubtractVector + VectorLength),
 * plus fixed zoom (0x52E), phase from the actor's yaw byte (rotZ), and
 * height from its +0x14 Z.
 * Identical body in ten level overlays.
 */

#include "vecmath.h"
#include "level_globals.h"
#include "actor.h"
#include "level_emit_particles.h"
#include "level_actor_cutscene.h"


/* The three position reads stay in byte-offset form: written as
 * actor->posX/posY/posZ (or through &actor->posX) sched1 reorders them
 * against the global stores and the function stops matching. */
void Level6_BeginActorCutscene(Actor *actor) {
  int vec[4];
  int *pos;
  int *p;
  int count;
  int b;

  p = actor->state;
  g_nGamestate = 0xC;
  g_nGamestate0cSubstate = 0;
  g_nGamestate0cTimer = 0;
  g_nGamestate0cMenuCursor = 0;
  g_pGamestate0cActor = (unsigned char *)actor;
  count = *p;
  pos = g_anCameraPos;
  g_nGameplayBlocked = 1;
  g_nGamePaused = 1;
  g_pGamestate0cRecord = g_pActorListBase + count * 0x58;
  g_nGamestate0cCamYawStart = ArcTan2(pos[0] - *(int *)((char *)actor + 0xC),
                             g_nCameraPosY - *(int *)((char *)actor + 0x10), 1);
  SubtractVector(vec, pos, &actor->posX);
  g_nGamestate0cCamRadiusStart = VectorLength(vec, 0);
  g_nGamestate0cCamHeightStart = g_nCameraPosZ;
  b = actor->rotZ;
  g_nGamestate0cCamRadiusGoal = 0x52E;
  g_nGamestate0cCamYawGoal = ((b << 4) - 0x312) & 0xFFF;
  g_nGamestate0cCamHeightGoal = *(int *)((char *)actor + 0x14) + 0x58F;
}
