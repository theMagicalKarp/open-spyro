/* func_level_18_8007B664 (0x8007B664, level_18_beast_makers_home overlay).
 *
 * Begin the level's gem/dragon cutscene focused on `actor`: enter gamestate
 * 0xC, clear the cutscene timers (D_800777E8..F0), record the focus actor
 * (D_8007784C) and its path end (D_80077850 = actor pool + count*0x58),
 * then aim the cutscene camera: yaw from the XY delta between the camera
 * anchor (D_80076DF8..) and the actor position (arctan via func_80016AB4),
 * pitch/roll from the transformed offset (func_8001778C + func_800171FC),
 * plus fixed zoom (0x52E), phase from the actor's +0x46 angle byte, and
 * height from its +0x14 Z.
 * Identical body in ten level overlays.
 */

extern int func_80016AB4(int dy, int dx, int flag);
extern void func_8001778C(int *out, int *a, int *b);
extern int func_800171FC(int *vec, int flag);

extern int D_800757D8; /* g_Gamestate */
extern int D_800777E8;
extern int D_800777EC;
extern int D_800777F0;
extern unsigned char *D_8007784C;
extern int D_8007570C;
extern int D_8007579C;
extern char *D_80075828;
extern char *D_80077850;
extern int D_80076DF8[];
extern int D_80076DFC;
extern int D_80076E00;
extern int D_80077820;
extern int D_80077824;
extern int D_80077828;
extern int D_80077838;
extern int D_8007783C;
extern int D_80077840;

void func_level_18_8007B664(unsigned char *actor) {
  int vec[4];
  int *pos;
  int *p;
  int count;
  int b;

  p = *(int **)actor;
  D_800757D8 = 0xC;
  D_800777E8 = 0;
  D_800777EC = 0;
  D_800777F0 = 0;
  D_8007784C = actor;
  count = *p;
  pos = D_80076DF8;
  D_8007570C = 1;
  D_8007579C = 1;
  D_80077850 = D_80075828 + count * 0x58;
  D_80077820 = func_80016AB4(pos[0] - *(int *)(actor + 0xC),
                             D_80076DFC - *(int *)(actor + 0x10), 1);
  func_8001778C(vec, pos, (int *)(actor + 0xC));
  D_80077824 = func_800171FC(vec, 0);
  D_80077828 = D_80076E00;
  b = actor[0x46];
  D_8007783C = 0x52E;
  D_80077838 = ((b << 4) - 0x312) & 0xFFF;
  D_80077840 = *(int *)(actor + 0x14) + 0x58F;
}
