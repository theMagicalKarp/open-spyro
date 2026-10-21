/* func_level_6_8007B9B4 (0x8007B9B4, level_6_peace_keepers_home overlay).
 *
 * Level hook run when leaving the save menu back to the world: forwards to
 * EndSaveMenuToWorld (EndSaveMenuToWorld), clears the level's pending-action word
 * (g_nSpyroSwingEulerPrev1) and flag byte (g_bSpyroSwingByteEuler1), then if the tracked actor
 * (g_pLevelOverlayInitRecord) exists, resets its +0x50 mode byte to 0x10.
 * Identical body in ten level overlays.
 */

extern void EndSaveMenuToWorld(void);
extern char *g_pLevelOverlayInitRecord;
extern int g_nSpyroSwingEulerPrev1;
extern unsigned char g_bSpyroSwingByteEuler1;

void func_level_6_8007B9B4(void) {
  char *actor;

  EndSaveMenuToWorld();
  actor = g_pLevelOverlayInitRecord;
  g_nSpyroSwingEulerPrev1 = 0;
  g_bSpyroSwingByteEuler1 = 0;
  if (actor != 0) {
    actor[0x50] = 0x10;
  }
}
