#include "actor.h"
#include "globals.h"

/* Reset an actor's animation, render and activation fields to their defaults
   (0x8003a720, 0x7c): anim/frame state cleared with frameRate 0x20 and
   nextFrame 1, euler bytes and mode/subMode zeroed, flags/groundCode/tint
   words cleared, otBias 4, renderList 0x20, cullRadius 0x10, group, visZone,
   updateDist and dropKind 0xFF (visZone 0xFF = always drawn, updateDist the
   widest activation radius, dropKind the random gem roll) and voiceRow 0x7F. */
void InitActorRenderDefaults(Actor *a) {
  a->cullRadius = 0x10;
  a->updateDist = 0xff;
  a->otBias = 4;
  a->nextFrame = 1;
  a->mode = 0;
  a->subMode = 0;
  a->anim = 0;
  a->nextAnim = 0;
  a->frame = 0;
  a->frameProgress = 0;
  a->frameRate = 0x20;
  a->group = 0xff;
  a->visZone = 0xff;
  a->dropKind = 0xff;
  a->flags = 0;
  a->rotX = 0;
  a->rotY = 0;
  a->rotZ = 0;
  a->groundCode = 0;
  a->scale = 0;
  a->voiceRow = VOICE_ROW_NONE;
  a->floorOffset = 0;
  a->renderList = 0x20;
  *(int *)&a->tint0 = 0;
}
