#include "globals.h"
#include "main_shared.h"

/* Step a countdown value of the given width (4/2/1 bytes) down by the global
 * delta g_nActorFrameDelta, clamping at zero. Returns:
 *   0 - still counting down (value was above the delta)
 *   2 - reached zero on this call (value was positive, now clamped to 0)
 *   1 - already at zero (nothing to do) */
int TickCountdownByFrameDelta(void *pValue, int width) {
  if (width == 4) {
    if (*((int *)pValue) <= g_nActorFrameDelta) {
      if (*((int *)pValue) == 0) {
        goto already_zero;
      }
      *((int *)pValue) = 0;
      return 2;
    }
    *((int *)pValue) = *((int *)pValue) - g_nActorFrameDelta;
    return 0;
  }

  if (width == 2) {
    if (*((short *)pValue) <= g_nActorFrameDelta) {
      if (*((short *)pValue) == 0) {
        goto already_zero;
      }
      *((short *)pValue) = 0;
      return 2;
    }
    *((short *)pValue) = *((short *)pValue) - g_nActorFrameDelta;
    return 0;
  }

  if (width == 1) {
    if (*((char *)pValue) <= g_nActorFrameDelta) {
      if (*((char *)pValue) != 0) {
        *((char *)pValue) = 0;
        return 2;
      }
    already_zero:
      return 1;
    }
    *((char *)pValue) = *((char *)pValue) - g_nActorFrameDelta;
    return 0;
  }
}
