#include "funcs.h"
#include "globals.h"

/* Trigger the actor's mesh-defined voice sample (sample id at +4 of the mesh
   record g_apActorMeshTable[actor->meshId] offset by voiceOffset). With an
   explicit owner (owner) the one-shot keys to it; otherwise the actor's own
   prior voices are stopped first and it keys to itself (+0x54). */
void PlayActorMeshVoice(Actor *actor, int voiceOffset, void *owner) {
  if (owner != 0) {
    PlaySoundEffect(
        ((unsigned char *)(&g_apActorMeshTable)[actor->type] + voiceOffset)[4],
        actor, 8, owner);
  } else {
    StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
    PlaySoundEffect(
        ((unsigned char *)(&g_apActorMeshTable)[actor->type] + voiceOffset)[4],
        actor, 8, &actor->sfxSlot);
  }
}
