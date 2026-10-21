/* func_level_3_8007AF2C (0x8007AF2C, level_3_artisans_town_square overlay).
 *
 * Begin the level's gem/dragon cutscene focused on `actor`: enter gamestate
 * 0xC, clear the cutscene timers (g_nGamestate0cSubstate..F0), record the focus actor
 * (D_8007784C) and its path end (D_80077850 = actor pool + count*0x58),
 * then aim the cutscene camera: yaw from the XY delta between the camera
 * anchor (g_anCameraPos..) and the actor position (arctan via ArcTan2),
 * pitch/roll from the transformed offset (SubtractVector + VectorLength),
 * plus fixed zoom (0x52E), phase from the actor's +0x46 angle byte, and
 * height from its +0x14 Z.
 * Identical body in ten level overlays.
 */

extern int ArcTan2(int dy, int dx, int flag);
extern void SubtractVector(int *out, int *a, int *b);
extern int VectorLength(int *vec, int flag);

extern int g_nGamestate; /* g_Gamestate */
extern int g_nGamestate0cSubstate;
extern int g_nGamestate0cTimer;
extern int D_800777F0;
extern unsigned char *D_8007784C;
extern int g_nGameplayBlocked;
extern int g_nGamePaused;
extern char *g_pActorListBase;
extern char *D_80077850;
extern int g_anCameraPos[];
extern int D_80076DFC;
extern int D_80076E00;
extern int D_80077820;
extern int D_80077824;
extern int D_80077828;
extern int D_80077838;
extern int D_8007783C;
extern int D_80077840;

void func_level_3_8007AF2C(unsigned char *actor) {
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
  D_8007784C = actor;
  count = *p;
  pos = g_anCameraPos;
  g_nGameplayBlocked = 1;
  g_nGamePaused = 1;
  D_80077850 = g_pActorListBase + count * 0x58;
  D_80077820 = ArcTan2(pos[0] - *(int *)(actor + 0xC),
                             D_80076DFC - *(int *)(actor + 0x10), 1);
  SubtractVector(vec, pos, (int *)(actor + 0xC));
  D_80077824 = VectorLength(vec, 0);
  D_80077828 = D_80076E00;
  b = actor[0x46];
  D_8007783C = 0x52E;
  D_80077838 = ((b << 4) - 0x312) & 0xFFF;
  D_80077840 = *(int *)(actor + 0x14) + 0x58F;
}
