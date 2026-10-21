/* func_level_7_8007B854 (0x8007B854, level_7_peace_keepers_dry_canyon overlay).
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

void func_level_7_8007B854(void) {
  char *actor;

  EndSaveMenuToWorld();
  actor = g_pLevelOverlayInitRecord;
  g_nSpyroSwingEulerPrev1 = 0;
  g_bSpyroSwingByteEuler1 = 0;
  if (actor != 0) {
    actor[0x50] = 0x10;
  }
}
