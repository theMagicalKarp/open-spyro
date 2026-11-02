#include "cd_music.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "spu.h"
#include "spyro_block.h"

/* 0x80056b28 (0x15c) — stop every active software voice whose bit is NOT set
   in `mask` (low 24 bits). Each stopped voice is accumulated into the pending
   hardware key-off word; mode-4 (one-shot) and mode-8 (owner-attached) rows
   also silence the owner's live sample byte (Spyro's g_bSpyroSfxVoiceMarker /
   Actor voiceRow). The row is then parked idle exactly like
   StopSoundVoicesByOwner (flags 0x40, owner and sample pointer cleared,
   priority 0xFF). Finally the pending key-on word is dropped and the music
   engine is told to stop (command 4). */
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
  int rowOff;
  int bit;
  int eight;
  int silence;
  volatile unsigned int *keyoff;
  mask = mask ^ 0xFFFFFF;
  i = 0;
  keyoff = g_adwSpuPendingKeyOffBlock;
  one = 1;
  eight = 8;
  silence = VOICE_ROW_NONE;
  off = 0;
top:
  if (((VOICE_ROW_RAW(off, unsigned short, nFlags)) & 0x83) != 0) {
    int rowAt;
    int mode;
    int row;
    int parkOff;
    unsigned char *owner;
    bit = one << i;
    if (((mask >> i) & 1) != 0) {
      rowOff = (i & 0x7F) * 0x1C;
      *keyoff |= bit;
      rowAt = rowOff;
      mode = (VOICE_ROW_RAW(rowAt, unsigned short, nFlags)) & SFX_MODE_MASK;
      if (mode == eight) { /* SFX_MODE_3D */
        goto mode3d;
      }
      if (mode < 9) {
        if (mode == SFX_MODE_FLAT) {
          owner = VOICE_ROW_RAW(rowAt, unsigned char *, pOwner);
          if (owner != 0) {
            owner[SPYB_VOICE_MARKER] = silence; /* every SFX_MODE_FLAT caller
                                               owns the row by Spyro's block */
          }
        }
      }
      goto park;
    mode3d:
      owner = VOICE_ROW_RAW(rowAt, unsigned char *, pOwner);

      if (owner != 0) {
        ((Actor *)owner)->voiceRow = silence;
      }
    park:
      row = i & 0x7F;

      parkOff = row * 0x1C;
      VOICE_ROW_RAW(parkOff, unsigned short, nFlags) = 0x40;
      VOICE_ROW_RAW(parkOff, int, pOwner) = 0;
      VOICE_ROW_RAW(parkOff, int, pPlaying) = 0;
      VOICE_ROW_RAW(parkOff, unsigned char, nSampleId) = 0xFF;
    }
  }

  i += 1;
  off += 0x1C;
  if (i < 0x18) {
    goto top;
  }
  g_dwSpuPendingKeyOn = 0;
  HandleMusicCommand(0, MUSIC_CMD_STOP_4);
}
