/* func_level_6_8007B9B4 (0x8007B9B4, level_6_peace_keepers_home overlay).
 *
 * Level hook run when leaving the save menu back to the world: forwards to
 * EndSaveMenuToWorld (func_8002D02C), clears the level's pending-action word
 * (D_80078C0C) and flag byte (D_80078A69), then if the tracked actor
 * (D_80075898) exists, resets its +0x50 mode byte to 0x10.
 * Identical body in ten level overlays.
 */

extern void func_8002D02C(void);
extern char *D_80075898;
extern int D_80078C0C;
extern unsigned char D_80078A69;

void func_level_6_8007B9B4(void) {
  char *actor;

  func_8002D02C();
  actor = D_80075898;
  D_80078C0C = 0;
  D_80078A69 = 0;
  if (actor != 0) {
    actor[0x50] = 0x10;
  }
}
