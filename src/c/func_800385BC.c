#include "globals.h"

extern int PlaySoundEffect(unsigned int vol, int param2, unsigned int mode,
                           void *owner);
extern void func_800530C0(int arg0, int arg1);

/* Play the actor's voice sample (sample id at sample-bank +0x26): a live actor
   (arg0 != 0) keys the one-shot to itself (mode 8, owner key arg0+0x54); the
   null actor plays it positionless (mode 0x10). Then run func_800530C0. */
void func_800385BC(int arg0, int arg1) {
  if (arg0 != 0) {
    PlaySoundEffect(((unsigned char *)g_pLevelSampleBankHeader)[0x26], arg0, 8,
                    (void *)(arg0 + 0x54));
  } else {
    PlaySoundEffect(((unsigned char *)g_pLevelSampleBankHeader)[0x26], 0, 0x10,
                    (void *)0);
  }
  func_800530C0(arg0, arg1);
}
