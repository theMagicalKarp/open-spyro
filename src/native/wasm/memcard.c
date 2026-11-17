/* src/native/wasm/memcard.c — the libmcrd upper layer.
 *
 * PsyZ has no libmcrd, so the calls the game makes (under its project
 * names: MemCardLoad is MemCardAccept, RunMemCardInfoOp is MemCardExist) are
 * implemented here over a raw 128 KiB memory-card image, the `.mcd` format
 * emulators use: block 0 holds the directory, blocks 1-15 the files, 8 KiB
 * each, read and written in 128-byte frames. The host names the image file
 * for each port (host_card_path); a port without one has no card. A missing
 * file is a freshly formatted card, written out on the first change.
 *
 * Every operation completes at once: MemCardSync reports it on the next
 * call. Async operations that fail report their error through MemCardSync,
 * as libmcrd does; CreateFile and Format are synchronous. */

#include <stdio.h>
#include <string.h>

#include "host_imports.h"

/* libmcrd result codes (include/memcard.h) and MemCardSync command ids. */
enum {
  McErrNone = 0,
  McErrCardNotExist = 1,
  McErrCardInvalid = 2,
  McErrNotFormat = 4,
  McErrFileNotExist = 5,
  McErrAlreadyExist = 6,
  McErrBlockFull = 7,
};
enum {
  McFuncExist = 1,
  McFuncAccept = 2,
  McFuncReadFile = 3,
  McFuncWriteFile = 4,
};

enum {
  CARD_SIZE = 128 * 1024,
  FRAME = 128,
  BLOCK = 8192,
  N_BLOCKS = 16, /* block 0: directory */
  NAME_LEN = 21,
};

/* Directory frame (block 0, frame n describes block n). Its next-block
   pointer holds the block number - 1. */
enum {
  DIR_FREE = 0xA0,
  DIR_FIRST = 0x51,
  DIR_MIDDLE = 0x52,
  DIR_LAST = 0x53,
  DIR_NO_NEXT = 0xFFFF,
};

typedef struct {
  int loaded;
  int present;
  char path[1024];
  unsigned char data[CARD_SIZE];
} Card;

static Card cards[2];

static int mc_cmd = -1;
static int mc_result;

static unsigned char *frame(Card *c, int block, int n) {
  return c->data + block * BLOCK + n * FRAME;
}

static void frame_checksum(unsigned char *f) {
  unsigned char x = 0;

  for (int i = 0; i < FRAME - 1; i++) {
    x ^= f[i];
  }
  f[FRAME - 1] = x;
}

static unsigned get32(const unsigned char *p) {
  return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24;
}

static void put32(unsigned char *p, unsigned v) {
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
  p[2] = (unsigned char)(v >> 16);
  p[3] = (unsigned char)(v >> 24);
}

static void set_dir(unsigned char *f, int state, unsigned size, int next,
                    const char *name) {
  memset(f, 0, FRAME);
  put32(f, (unsigned)state);
  put32(f + 4, size);
  f[8] = (unsigned char)next;
  f[9] = (unsigned char)(next >> 8);
  if (name != NULL) {
    strncpy((char *)f + 0x0A, name, NAME_LEN - 1);
  }
  frame_checksum(f);
}

/* The layout a PS1 formats a card to. */
static void card_format(Card *c) {
  memset(c->data, 0, CARD_SIZE);
  for (int n = 0; n < 64; n++) {
    unsigned char *f = frame(c, 0, n);

    if (n == 0 || n == 63) { /* header and write-test frame */
      f[0] = 'M';
      f[1] = 'C';
    } else if (n < N_BLOCKS) {
      set_dir(f, DIR_FREE, 0, DIR_NO_NEXT, NULL);
      continue;
    } else if (n < 36) { /* broken-sector list: none */
      put32(f, 0xFFFFFFFF);
      f[8] = f[9] = 0xFF;
    }
    frame_checksum(f);
  }
}

static int card_formatted(Card *c) {
  return c->data[0] == 'M' && c->data[1] == 'C';
}

static void card_save(Card *c) {
  FILE *fp = fopen(c->path, "wb");

  if (fp == NULL || fwrite(c->data, CARD_SIZE, 1, fp) != 1) {
    printf("memcard: cannot write %s\n", c->path);
  }
  if (fp != NULL) {
    fclose(fp);
  }
}

/* The card in `chan` (port << 4 | slot; only slot 0 exists), or NULL. */
static Card *card(int chan) {
  int port = (chan >> 4) & 1;
  Card *c = &cards[port];

  if ((chan & 0xF) != 0) {
    return NULL;
  }
  if (!c->loaded) {
    c->loaded = 1;
    c->present = host_card_path(port, c->path, sizeof(c->path));
    if (c->present) {
      FILE *fp = fopen(c->path, "rb");

      if (fp == NULL) {
        card_format(c);
      } else {
        if (fread(c->data, CARD_SIZE, 1, fp) != 1) {
          printf("memcard: %s is not a 128 KiB card image\n", c->path);
          c->present = 0;
        }
        fclose(fp);
      }
    }
  }
  return c->present ? c : NULL;
}

/* First block of the file `name`, or 0. */
static int find_file(Card *c, const char *name) {
  for (int b = 1; b < N_BLOCKS; b++) {
    unsigned char *f = frame(c, 0, b);

    if (get32(f) == DIR_FIRST &&
        strncmp((const char *)f + 0x0A, name, NAME_LEN) == 0) {
      return b;
    }
  }
  return 0;
}

/* Copies `size` bytes at byte `ofs` of a file's block chain to / from buf. */
static int file_io(Card *c, int block, unsigned char *buf, int ofs, int size,
                   int write) {
  while (size > 0) {
    unsigned char *dir = frame(c, 0, block);

    if (ofs < BLOCK) {
      int n = BLOCK - ofs < size ? BLOCK - ofs : size;
      unsigned char *p = c->data + block * BLOCK + ofs;

      if (write) {
        memcpy(p, buf, (size_t)n);
      } else {
        memcpy(buf, p, (size_t)n);
      }
      buf += n;
      size -= n;
      ofs = 0;
    } else {
      ofs -= BLOCK;
    }
    if (size > 0) {
      int next = dir[8] | dir[9] << 8;

      if (next >= N_BLOCKS - 1) {
        return McErrCardInvalid;
      }
      block = next + 1;
    }
  }
  return McErrNone;
}

static void mc_finish(int cmd, int result) {
  mc_cmd = cmd;
  mc_result = result;
}

void MemCardReset(int mode) { (void)mode; }

void MemCardOpenSession(void) {}

void MemCardCloseSession(void) {}

/* MemCardExist: is a card inserted. */
void RunMemCardInfoOp(int chan) {
  mc_finish(McFuncExist, card(chan) ? McErrNone : McErrCardNotExist);
}

/* MemCardAccept: is a card inserted, and formatted. */
void MemCardLoad(int chan) {
  Card *c = card(chan);

  mc_finish(McFuncAccept, c == NULL            ? McErrCardNotExist
                          : !card_formatted(c) ? McErrNotFormat
                                               : McErrNone);
}

static int file_op(int chan, const char *name, void *buf, int ofs, int size,
                   int write) {
  Card *c = card(chan);
  int block;

  if (c == NULL) {
    return McErrCardNotExist;
  }
  if (!card_formatted(c)) {
    return McErrNotFormat;
  }
  block = find_file(c, name);
  if (block == 0) {
    return McErrFileNotExist;
  }
  if (ofs < 0 || size < 0 ||
      (unsigned)ofs + (unsigned)size > get32(frame(c, 0, block) + 4)) {
    return McErrCardInvalid;
  }
  int r = file_io(c, block, buf, ofs, size, write);
  if (r == McErrNone && write) {
    card_save(c);
  }
  return r;
}

void MemCardRead(int chan, char *name, void *buf, int ofs, int size) {
  mc_finish(McFuncReadFile, file_op(chan, name, buf, ofs, size, 0));
}

void MemCardWrite(int chan, char *name, void *buf, int ofs, int size) {
  mc_finish(McFuncWriteFile, file_op(chan, name, buf, ofs, size, 1));
}

/* Mode 0 waits, mode 1 polls; both find the command already finished.
   Returns 1 with it, or -1 when none is registered. */
int MemCardSync(int mode, int *cmd, int *result) {
  (void)mode;
  if (mc_cmd < 0) {
    return -1;
  }
  if (cmd) {
    *cmd = mc_cmd;
  }
  if (result) {
    *result = mc_result;
  }
  mc_cmd = -1;
  return 1;
}

int MemCardCreateFile(int chan, char *name, int blocks) {
  Card *c = card(chan);
  int free_blocks[N_BLOCKS], n = 0;

  if (c == NULL) {
    return McErrCardNotExist;
  }
  if (!card_formatted(c)) {
    return McErrNotFormat;
  }
  if (find_file(c, name) != 0) {
    return McErrAlreadyExist;
  }
  for (int b = 1; b < N_BLOCKS && n < blocks; b++) {
    if ((get32(frame(c, 0, b)) & 0xF0) == DIR_FREE) {
      free_blocks[n++] = b;
    }
  }
  if (blocks < 1 || n < blocks) {
    return McErrBlockFull;
  }
  for (int i = 0; i < n; i++) {
    int state = i == 0 ? DIR_FIRST : i == n - 1 ? DIR_LAST : DIR_MIDDLE;
    int next = i == n - 1 ? DIR_NO_NEXT : free_blocks[i + 1] - 1;

    set_dir(frame(c, 0, free_blocks[i]), state,
            i == 0 ? (unsigned)(blocks * BLOCK) : 0, next,
            i == 0 ? name : NULL);
    memset(c->data + free_blocks[i] * BLOCK, 0, BLOCK);
  }
  card_save(c);
  return McErrNone;
}

int MemCardFormat(int chan) {
  Card *c = card(chan);

  if (c == NULL) {
    return McErrCardNotExist;
  }
  card_format(c);
  card_save(c);
  return McErrNone;
}
