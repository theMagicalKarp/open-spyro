#include "funcs.h"
#include "globals.h"

/* Play voice `voice` of the actor's model: the sample id is byte 4 + voice
   of its g_apActorMeshTable record. With a `voiceRow` slot the sound keys to
   it; otherwise the actor's own voices are stopped first and it keys to the
   actor's ->voiceRow. */
void PlayActorMeshVoice(Actor *actor, int voice, void *voiceRow) {
  if (voiceRow != 0) {
    PlaySoundEffect(
        ((unsigned char *)(&g_apActorMeshTable)[actor->type] + voice)[4], actor,
        SFX_MODE_3D, voiceRow);
  } else {
    StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
    PlaySoundEffect(
        ((unsigned char *)(&g_apActorMeshTable)[actor->type] + voice)[4], actor,
        SFX_MODE_3D, &actor->voiceRow);
  }
}
