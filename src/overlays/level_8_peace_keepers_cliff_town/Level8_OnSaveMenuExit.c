/* Level8_OnSaveMenuExit (0x8007B854, level_8_peace_keepers_cliff_town overlay).
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

void Level8_OnSaveMenuExit(void) {
  char *actor;

  EndSaveMenuToWorld();
  actor = g_pLevelOverlayInitRecord;
  g_nSpyroSwingEulerPrev1 = 0;
  g_bSpyroSwingByteEuler1 = 0;
  if (actor != 0) {
    actor[0x50] = 0x10;
  }
}
