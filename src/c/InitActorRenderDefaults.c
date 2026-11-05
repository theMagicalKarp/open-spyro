#include "actor.h"
#include "globals.h"

/* Reset an actor's animation, render and activation fields to their defaults
   (0x8003a720, 0x7c): anim/frame state cleared with frameRate 0x20 and
   nextFrame 1, euler bytes and mode/subMode zeroed, flags/groundCode/tint
   words cleared, otBias 4, renderList 0x20, cullRadius 0x10, group, gateChunk,
   updateDist and dropKind 0xFF (gateChunk 0xFF = always drawn, updateDist the
   widest activation radius, dropKind the random gem roll) and voiceRow 0x7F. */
void InitActorRenderDefaults(Actor *actor) {
  actor->cullRadius = 0x10;
  actor->updateDist = ACTOR_UPDATE_ALWAYS;
  actor->otBias = 4;
  actor->nextFrame = 1;
  actor->mode = ACTOR_SPAWN_MODE;
  actor->subMode = 0;
  actor->anim = ACTOR_SPAWN_ANIM;
  actor->nextAnim = ACTOR_SPAWN_ANIM;
  actor->frame = 0;
  actor->frameProgress = 0;
  actor->frameRate = 0x20;
  actor->group = 0xff;
  actor->gateChunk = GATE_CHUNK_ALWAYS;
  actor->dropKind = DROP_GEM_ROLL;
  actor->flags = 0;
  actor->rotX = 0;
  actor->rotY = 0;
  actor->rotZ = 0;
  actor->groundCode = 0;
  actor->scale = 0;
  actor->voiceRow = VOICE_ROW_NONE;
  actor->floorOffset = 0;
  actor->renderList = 0x20;
  *(int *)&actor->tint0 = 0;
}
