#include "save_game.h"

/* 0x8005956c — the save image checksum: the byte sum of a SaveGameBuffer up
   to (not including) its checksum field. BuildSaveGameBuffer stamps it and the
   save menu / title screen compare it after a read. */
int ComputeSaveGameChecksum(unsigned char *data) {
  int sum = 0;
  unsigned char *end = data + SAVE_OFFSET(checksum);

  do {
    sum += *data;
    data++;
  } while (data < end);
  return sum;
}
