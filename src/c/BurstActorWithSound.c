#include "globals.h"

extern int PlaySoundEffect(unsigned int vol, int param2, unsigned int mode,
                           void *owner);
extern void BurstActorIntoSparks(int actor, int mode);

/* Play the actor's voice sample (sample id at sample-bank +0x26): a live actor
   (actor != 0) keys the one-shot to itself (mode 8, owner key actor+0x54); the
   null actor plays it positionless (mode 0x10). Then run BurstActorIntoSparks.
 */
void BurstActorWithSound(int actor, int mode) {
  if (actor != 0) {
    PlaySoundEffect(((unsigned char *)g_pLevelSampleBankHeader)[0x26], actor, 8,
                    (void *)(actor + 0x54));
  } else {
    PlaySoundEffect(((unsigned char *)g_pLevelSampleBankHeader)[0x26], 0, 0x10,
                    (void *)0);
  }
  BurstActorIntoSparks(actor, mode);
}
