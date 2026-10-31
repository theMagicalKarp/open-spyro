#include "funcs.h"
#include "globals.h"
#include "level_id.h"

/* Seed a fresh game: default level, vibration on, default camera options, then
   wipe per-level state. */
void InitNewGameState(void) {
  g_nActiveLevelId = LEVEL_ARTISANS;
  g_nOptionVibrationEnabled = 1;
  g_dwActiveCameraOptions = 0x52;
  ResetLevelState();
}
