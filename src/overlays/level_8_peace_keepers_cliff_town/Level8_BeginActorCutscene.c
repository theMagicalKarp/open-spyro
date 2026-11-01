/* Level8_BeginActorCutscene (0x8007B718, level_8_peace_keepers_cliff_town overlay).
 *
 * Begin the level's gem/dragon cutscene focused on `actor`: enter gamestate
 * 0xC, clear the cutscene timers (g_nActorCutsceneSubstate..F0), record the focus actor
 * (g_pActorCutsceneActor) and its path end (g_pActorCutsceneRecord = pool slot `count`),
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
#include "gamestate.h"


/* The three position reads go through ACTOR_RAW: written as
 * actor->posX/posY/posZ (or through &actor->posX) sched1 reorders them
 * against the global stores and the function stops matching. */
void Level8_BeginActorCutscene(Actor *actor) {
  int vec[4];
  int *pos;
  int *p;
  int count;
  int b;

  p = actor->state;
  g_nGamestate = GS_ACTOR_CUTSCENE;
  g_nActorCutsceneSubstate = ACS_APPROACH;
  g_nActorCutsceneTimer = 0;
  g_nActorCutsceneMenuCursor = 0;
  g_pActorCutsceneActor = actor;
  count = *p;
  pos = g_anCameraPos;
  g_nCinematicActive = 1;
  g_nGamePaused = 1;
  g_pActorCutsceneRecord = &g_pActorListBase[count];
  g_nActorCutsceneCamYawStart = ArcTan2(pos[0] - ACTOR_RAW(actor, int, posX),
                             g_nCameraPosY - ACTOR_RAW(actor, int, posY), 1);
  SubtractVector(vec, pos, &actor->posX);
  g_nActorCutsceneCamRadiusStart = VectorLength(vec, VEC_LEN_XY);
  g_nActorCutsceneCamHeightStart = g_nCameraPosZ;
  b = actor->rotZ;
  g_nActorCutsceneCamRadiusGoal = 0x52E;
  g_nActorCutsceneCamYawGoal = ((b << 4) - 0x312) & 0xFFF;
  g_nActorCutsceneCamHeightGoal = ACTOR_RAW(actor, int, posZ) + 0x58F;
}
