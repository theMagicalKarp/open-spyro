#include "funcs.h"
#include "globals.h"

/* Step byte angle `from` toward `to` by at most `maxStep`: if the shortest
   signed delta (from - to) is larger in magnitude than the limit `maxStep`,
   move `from` by `maxStep` in the delta's direction; otherwise it is already
   within range, so return `to`. */
int StepAngle8Toward(int to, int from, int maxStep, int unused) {
  int delta = WrapAngleDelta8(to, from);
  int absd = delta >= 0 ? delta : -delta;
  if (maxStep < absd) {
    if (delta < 0) {
      return WrapAddAngle8(from, maxStep);
    }
    return WrapAddAngle8(from, -maxStep);
  }
  return to;
}
