/* Level6_BeginActorCutscene (0x8007B878, level_6_peace_keepers_home overlay).
 *
 * Begin the level's gem/dragon cutscene focused on `actor`: enter gamestate
 * 0xC, clear the cutscene timers (g_nGamestate0cSubstate..F0), record the focus actor
 * (g_pGamestate0cActor) and its path end (g_pGamestate0cRecord = actor pool + count*0x58),
 * then aim the cutscene camera: yaw from the XY delta between the camera
 * anchor (g_anCameraPos..) and the actor position (arctan via ArcTan2),
 * pitch/roll from the transformed offset (SubtractVector + VectorLength),
 * plus fixed zoom (0x52E), phase from the actor's +0x46 angle byte, and
 * height from its +0x14 Z.
 * Identical body in ten level overlays.
 */

#include "vecmath.h"

extern int g_nGamestate; /* g_Gamestate */
extern int g_nGamestate0cSubstate;
extern int g_nGamestate0cTimer;
extern int D_800777F0;
extern unsigned char *g_pGamestate0cActor;
extern int g_nGameplayBlocked;
extern int g_nGamePaused;
extern char *g_pActorListBase;
extern char *g_pGamestate0cRecord;
extern int g_anCameraPos[];
extern int g_nCameraPosY;
extern int g_nCameraPosZ;
extern int g_nGamestate0cCamYawStart;
extern int g_nGamestate0cCamRadiusStart;
extern int g_nGamestate0cCamHeightStart;
extern int g_nGamestate0cCamYawGoal;
extern int g_nGamestate0cCamRadiusGoal;
extern int g_nGamestate0cCamHeightGoal;

void Level6_BeginActorCutscene(unsigned char *actor) {
  int vec[4];
  int *pos;
  int *p;
  int count;
  int b;

  p = *(int **)actor;
  g_nGamestate = 0xC;
  g_nGamestate0cSubstate = 0;
  g_nGamestate0cTimer = 0;
  D_800777F0 = 0;
  g_pGamestate0cActor = actor;
  count = *p;
  pos = g_anCameraPos;
  g_nGameplayBlocked = 1;
  g_nGamePaused = 1;
  g_pGamestate0cRecord = g_pActorListBase + count * 0x58;
  g_nGamestate0cCamYawStart = ArcTan2(pos[0] - *(int *)(actor + 0xC),
                             g_nCameraPosY - *(int *)(actor + 0x10), 1);
  SubtractVector(vec, pos, (int *)(actor + 0xC));
  g_nGamestate0cCamRadiusStart = VectorLength(vec, 0);
  g_nGamestate0cCamHeightStart = g_nCameraPosZ;
  b = actor[0x46];
  g_nGamestate0cCamRadiusGoal = 0x52E;
  g_nGamestate0cCamYawGoal = ((b << 4) - 0x312) & 0xFFF;
  g_nGamestate0cCamHeightGoal = *(int *)(actor + 0x14) + 0x58F;
}
