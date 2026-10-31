#include "funcs.h"
#include "globals.h"

/* Step byte angle `from` toward `to` by limit `maxStep` (StepAngle8Toward),
   then ease the result the rest of the way by fraction `frac`/256 (LerpAngle8).
 */
int StepAndLerpAngle8(int to, int from, int maxStep, int frac) {
  return LerpAngle8(StepAngle8Toward(to, from, maxStep, frac), from,
                    frac & 0xFF);
}
