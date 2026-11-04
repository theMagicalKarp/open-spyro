/* memcard.h — PSY-Q libmcrd result codes (MemCardSync / MemCardLoad / ...)
 * and the game's memory-card save file: the standard 0x200-byte PS1 file
 * header followed by three 0x600-byte save images. */

#ifndef OPEN_SPYRO_MEMCARD_H
#define OPEN_SPYRO_MEMCARD_H

#define McErrNone 0
#define McErrCardNotExist 1
#define McErrCardInvalid 2
#define McErrNewCard 3
#define McErrNotFormat 4
#define McErrFileNotExist 5
#define McErrAlreadyExist 6
#define McErrBlockFull 7

#include "save_game.h"

typedef struct {
  char magic[2];
  unsigned char iconFlag;
  unsigned char blockCount;
  unsigned char title[0x40];
  char reserved[0x1C];
  unsigned short clut[0x10]; /* clut[15] holds the file tag (g_nSaveFileTag) the
                       * title screen stamps at creation; the save menu
                       * checks it before writing */
  unsigned char icon[0x80][3];
} McFileHeader; /* 0x200 */

typedef struct {
  McFileHeader header;     /* 0x000 */
  SaveGameBuffer saves[3]; /* 0x200, 3 x 0x600 */
} MemCardSaveFile;

/* Byte offset of save slot `i` in the card file. */
#define MEMCARD_SAVE_OFFSET(i) ((int)&((MemCardSaveFile *)0)->saves[i])

#endif /* OPEN_SPYRO_MEMCARD_H */
