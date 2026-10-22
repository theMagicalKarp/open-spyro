#include "globals.h"

extern int PlaySoundEffect(unsigned int vol, void *param2, unsigned int mode,
                           void *owner);
extern void StopSoundVoicesByOwner(void *owner, int flag);

/* Trigger the actor's mesh-defined voice sample (sample id at +4 of the mesh
   record g_apActorMeshTable[actor->meshId] offset by voiceOffset). With an
   explicit owner (owner) the one-shot keys to it; otherwise the actor's own
   prior voices are stopped first and it keys to itself (+0x54). */
void PlayActorMeshVoice(int *e, int voiceOffset, void *owner) {
  if (owner != 0) {
    PlaySoundEffect(
        ((unsigned char *)(&g_apActorMeshTable)[((short *)e)[0x1B]] +
         voiceOffset)[4],
        e, 8, owner);
  } else {
    StopSoundVoicesByOwner(e, 1);
    PlaySoundEffect(
        ((unsigned char *)(&g_apActorMeshTable)[((short *)e)[0x1B]] +
         voiceOffset)[4],
        e, 8, (char *)e + 0x54);
  }
}
