#include "globals.h"

/* Empty stub at 0x80037E98 — bare `jr $ra; nop`. Every animation queue in
   the actor code (the ANIM_* / ENTER_POSE macros and their open-coded
   copies) calls it right after setting nextAnim/nextFrame. */
void OnActorAnimQueued(void) {}
