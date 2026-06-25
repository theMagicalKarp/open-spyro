#include "globals.h"

extern int PlaySoundEffect(unsigned int vol, void *param2, unsigned int mode,
                           void *owner);
extern void StopSoundVoicesByOwner(void *owner, int flag);

/* Trigger the actor's mesh-defined voice sample (sample id at +4 of the mesh
   record g_apActorMeshTable[actor->meshId] offset by arg1). With an explicit
   owner (arg2) the one-shot keys to it; otherwise the actor's own prior voices
   are stopped first and it keys to itself (+0x54). */
void func_8003851C(int *e, int arg1, void *arg2) {
  if (arg2 != 0) {
    PlaySoundEffect(
        ((unsigned char *)(&g_apActorMeshTable)[((short *)e)[0x1B]] + arg1)[4],
        e, 8, arg2);
  } else {
    StopSoundVoicesByOwner(e, 1);
    PlaySoundEffect(
        ((unsigned char *)(&g_apActorMeshTable)[((short *)e)[0x1B]] + arg1)[4],
        e, 8, (char *)e + 0x54);
  }
}
