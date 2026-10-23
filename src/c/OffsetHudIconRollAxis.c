#include "actor_type.h"
#include "globals.h"

/* Icon record at byte offset `o` (slot * 0x58) in the HUD record array. */
#define ICON(o) ((Actor *)((char *)g_abHudIconActorRecords + (o)))

/* Shift the position of `count` consecutive HUD icon actors by `delta`,
   starting at icon slot `base`. Rest poses live in a 12-byte-per-record table
   (word at +4), and the live values are written into the 88-byte icon actor
   records (word at +0x10). (0x80054400, 92 bytes.) */
void OffsetHudIconRollAxis(int base, int count, int delta) {
  int i;
  for (i = 0; i < count; i++) {
    int o1 = (base + i) * 12;
    int o2 = (base + i) * 88;
    ICON(o2)->posY = *(int *)((char *)g_anHudIconRestPos + o1 + 4) + delta;
  }
}
