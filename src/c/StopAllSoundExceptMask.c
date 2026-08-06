#include "globals.h"

extern void HandleMusicCommand(int a, int b);

extern unsigned int g_adwSpuPendingKeyOffBlock[];

/* 0x80056b28 (0x15c) — stop every active software voice whose bit is NOT set
   in `mask` (low 24 bits). Each stopped voice is accumulated into the pending
   hardware key-off word; mode-4 (one-shot) and mode-8 (owner-attached) rows
   also silence the owner's live sample byte (+0x2A0 / +0x54). The row is then
   parked idle exactly like StopSoundVoicesByOwner (flags 0x40, owner and
   sample pointer cleared, priority 0xFF). Finally the pending key-on word is
   dropped and the music engine is told to stop (command 4). */
/* The row bit is computed BETWEEN the two guards rather than inside the body:
   it is what reorg puts in the second beqz's delay slot, and it can only get
   there from the guard block. Written inside the body it never reaches the
   slot - the body's first insn is then the volatile key-off load, which reorg
   will not execute speculatively, so the slot takes a nop and the shift falls
   into the load shadow instead. (sched1 orders them that way for a reason no
   source form inside the body can undo: the load and the shift feed the same
   `or`, both are birthing defs, and after the `or` is scheduled the load
   becomes ready one cycle LATER than the shift, so the shift wins the earlier
   slot and the load heads the block.) */

void StopAllSoundExceptMask(int mask) {
  int i;
  int off;
  int one;
  int new_var;
  int bit;
  int eight;
  int sil;
  volatile unsigned int *keyoff;
  mask = mask ^ 0xFFFFFF;
  i = 0;
  keyoff = g_adwSpuPendingKeyOffBlock;
  one = 1;
  eight = 8;
  sil = 0x7F;
  off = 0;
top:
  if (((*((unsigned short *)((((char *)g_abSpuCommonAttr) + off) + 0x36))) &
       0x83) != 0) {
    int o;
    int m;
    int v2;
    int o2;
    unsigned char *owner;
    bit = one << i;
    if (((mask >> i) & 1) != 0) {
      new_var = (i & 0x7F) * 0x1C;
      *keyoff |= bit;
      o = new_var;
      m = (*((unsigned short *)((((char *)g_abSpuCommonAttr) + o) + 0x36))) &
          0x1C;
      if (m == eight) {
        goto mode8;
      }
      if (m < 9) {
        if (m == 4) {
          owner =
              *((unsigned char **)((((char *)g_abSpuCommonAttr) + o) + 0x28));
          if (owner != 0) {
            owner[0x2A0] = sil;
          }
        }
      }
      goto park;
    mode8:
      owner = *((unsigned char **)((((char *)g_abSpuCommonAttr) + o) + 0x28));

      if (owner != 0) {
        owner[0x54] = sil;
      }
    park:
      v2 = i & 0x7F;

      o2 = v2 * 0x1C;
      *((unsigned short *)((((char *)g_abSpuCommonAttr) + o2) + 0x36)) = 0x40;
      *((int *)((((char *)g_abSpuCommonAttr) + o2) + 0x28)) = 0;
      *((int *)((((char *)g_abSpuCommonAttr) + o2) + 0x40)) = 0;
      *((unsigned char *)((((char *)g_abSpuCommonAttr) + o2) + 0x35)) = 0xFF;
    }
  }

  i += 1;
  off += 0x1C;
  if (i < 0x18) {
    goto top;
  }
  g_dwSpuPendingKeyOn = 0;
  HandleMusicCommand(0, 4);
}
