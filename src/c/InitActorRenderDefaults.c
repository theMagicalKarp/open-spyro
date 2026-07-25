#include "globals.h"

/* Zero/default initializer for an actor's render-state block (+0x18..+0x57).
   Seeds two POLY_F3 prim-type tags (0x20) at +0x41/+0x4b, full-bright RGB
   (0xff) at +0x43/+0x4a/+0x52/+0x53, fade 0x7f at +0x54, default counters
   0x10/+0x50, 4/+0x47, 1/+0x3f, and clears the byte yaw/pitch/roll bytes and
   the state words at +0x18/+0x1c/+0x38/+0x4c. (0x8003a720, 0x7c) */
void InitActorRenderDefaults(int actor) {
  int new_var;
  *((unsigned char *)(actor + 0x50)) = 0x10;
  *((unsigned char *)(actor + 0x52)) = 0xff;
  *((unsigned char *)(actor + 0x47)) = 4;
  *((unsigned char *)(actor + 0x3f)) = 1;
  *((unsigned char *)(actor + 0x48)) = 0;
  *((unsigned char *)(actor + 0x49)) = 0;
  *((unsigned char *)(actor + 0x3c)) = 0;
  *((unsigned char *)(actor + 0x3d)) = 0;
  *((unsigned char *)(actor + 0x3e)) = 0;
  *((unsigned char *)(actor + 0x40)) = 0;
  *((unsigned char *)(actor + 0x41)) = 0x20;
  *((unsigned char *)(actor + 0x43)) = 0xff;
  *((unsigned char *)(actor + 0x4a)) = 0xff;
  *((unsigned char *)(actor + 0x53)) = 0xff;
  new_var = actor;
  *((int *)(actor + 0x18)) = 0;
  *((unsigned char *)(actor + 0x44)) = 0;
  *((unsigned char *)(new_var + 0x45)) = 0;
  *((unsigned char *)(new_var + 0x46)) = 0;
  *((int *)(new_var + 0x1c)) = 0;
  *((unsigned char *)(actor + 0x57)) = 0;
  *((unsigned char *)(new_var + 0x54)) = 0x7f;
  *((short *)(new_var + 0x38)) = 0;
  *((unsigned char *)(actor + 0x4b)) = 0x20;
  *((int *)(actor + 0x4c)) = 0;
}
