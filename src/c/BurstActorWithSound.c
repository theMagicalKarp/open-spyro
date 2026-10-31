#include "funcs.h"
#include "globals.h"
#include "level_sfx.h"
#include "sample_bank.h"

/* Play the actor's voice sample (sample id at sample-bank +0x26): a live actor
   (actor != 0) keys the one-shot to itself (mode 8, owner key
   &actor->voiceRow); the null actor plays it positionless (mode 0x10). Then run
   BurstActorIntoSparks.
 */
void BurstActorWithSound(Actor *actor, int mode) {
  if (actor != 0) {
    PlaySoundEffect(LEVEL_SFX(SFX_ACTOR_BURST), actor, SFX_MODE_3D,
                    &actor->voiceRow);
  } else {
    PlaySoundEffect(LEVEL_SFX(SFX_ACTOR_BURST), 0, SFX_MODE_PANNED, (void *)0);
  }
  BurstActorIntoSparks(actor, mode);
}
