#include "globals.h"

/* 0x8001256c, 152 bytes — build the 6x8 music-track LBA table: every track
   entry gets its bank's base LBA and the LBA one track-length later. */

int BuildAudioTrackTable(void) {
  int pad[2];
  int bank;
  int track;
  int idx;
  int *lengths;

  bank = 0;
  lengths = g_anCdAudioTrackLengths;
  for (; bank < 6; bank++) {
    int *len;
    int row;

    for (track = 0, row = bank * 8, len = lengths; track < 8; track++) {
      idx = (row + track) * 8;
      *(int *)((char *)g_anMusicTrackLbaTable + idx) =
          g_anCdAudioBankBaseLba[bank];
      *(int *)((char *)g_anMusicTrackLbaTable + idx + 4) =
          g_anCdAudioBankBaseLba[bank] + *len;
      len++;
    }
    lengths += 8;
  }

  g_dwMusicStreamStatus = 0x40;
  return 1;
}
