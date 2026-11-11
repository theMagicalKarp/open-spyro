#include "globals.h"
#include "spyro_state.h"

/* Resolve the current ground surface to its trigger-region type byte. The low 6
   bits of g_nGroundSurfaceType index a table of region records; 0x3F is the
   "no region" sentinel. For a real region, dereference its record and return
   the leading type byte; otherwise return -1. */
int GetGroundTriggerRegionType(void) {
  int surface = g_nGroundSurfaceType & TRIGGER_REGION_MASK;

  if (surface != TRIGGER_REGION_NONE) {
    return *((unsigned char **)g_pTriggerRegionTable)[surface];
  }
  return -1;
}
