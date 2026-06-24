#include "globals.h"

extern void ResetLevelState(void);

/* Seed a fresh game: default level, vibration on, default camera options, then
   wipe per-level state. */
void InitNewGameState(void) {
  g_nActiveLevelId = 10;
  g_nOptionVibrationEnabled = 1;
  g_dwActiveCameraOptions = 0x52;
  ResetLevelState();
}
