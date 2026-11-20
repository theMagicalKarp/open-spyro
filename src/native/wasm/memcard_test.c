/* src/native/wasm/memcard_test.c — unit tests for memcard.c (make
 * native-test).
 *
 * memcard.c is included, not linked, so the tests can reset its card cache
 * and read the directory through its own helpers. */

#include "memcard.c"

#include <stdlib.h>

#include "test.h"

/* host_card_path (host_imports.h): the image for each port, NULL for none. */
static const char *card_paths[2];

int host_card_path(int port, char *buf, int len) {
  const char *path = card_paths[port];

  if (path == NULL || (int)strlen(path) >= len) {
    return 0;
  }
  strcpy(buf, path);
  return 1;
}

/* Forgets every loaded card (as at boot) and points port 0 at `path`,
   removing any image left there by an earlier run. */
static void reset(const char *path) {
  memset(cards, 0, sizeof(cards));
  mc_cmd = -1;
  card_paths[0] = path;
  card_paths[1] = NULL;
  if (path != NULL) {
    remove(path);
  }
}

/* Drops the loaded cards so the next call reads the image back from disk. */
static void reload(void) { memset(cards, 0, sizeof(cards)); }

/* The command and result MemCardSync reports, as cmd << 8 | result. */
static int sync_result(void) {
  int cmd = -1, result = -1;

  if (MemCardSync(0, &cmd, &result) != 1) {
    return -1;
  }
  return cmd << 8 | result;
}

static int frame_checksum_ok(const unsigned char *f) {
  unsigned char x = 0;

  for (int i = 0; i < FRAME; i++) {
    x ^= f[i];
  }
  return x == 0;
}

static long file_length(const char *path) {
  FILE *fp = fopen(path, "rb");
  long n;

  if (fp == NULL) {
    return -1;
  }
  fseek(fp, 0, SEEK_END);
  n = ftell(fp);
  fclose(fp);
  return n;
}

static void write_image(const char *path, int size, unsigned char fill) {
  FILE *fp = fopen(path, "wb");

  for (int i = 0; i < size; i++) {
    fputc(fill, fp);
  }
  fclose(fp);
}

#define CARD TEST_TMPDIR "/card.mcd"

/* --- presence ------------------------------------------------------------ */

TEST(memcard_sync_without_command) {
  int cmd = 7, result = 7;

  reset(NULL);
  CHECK_INT(MemCardSync(1, &cmd, &result), -1);
  CHECK_INT(cmd, 7);
  CHECK_INT(result, 7);
}

TEST(memcard_sync_reports_once) {
  reset(NULL);
  RunMemCardInfoOp(0x00);
  CHECK_INT(MemCardSync(0, NULL, NULL), 1);
  CHECK_INT(MemCardSync(0, NULL, NULL), -1);
}

TEST(memcard_no_card_path) {
  reset(NULL);
  RunMemCardInfoOp(0x00);
  CHECK_INT(sync_result(), McFuncExist << 8 | McErrCardNotExist);
  MemCardLoad(0x00);
  CHECK_INT(sync_result(), McFuncAccept << 8 | McErrCardNotExist);
  CHECK_INT(MemCardCreateFile(0x00, "BASLUS", 1), McErrCardNotExist);
  CHECK_INT(MemCardFormat(0x00), McErrCardNotExist);
}

TEST(memcard_only_slot_zero_exists) {
  reset(CARD);
  RunMemCardInfoOp(0x01);
  CHECK_INT(sync_result(), McFuncExist << 8 | McErrCardNotExist);
  RunMemCardInfoOp(0x00);
  CHECK_INT(sync_result(), McFuncExist << 8 | McErrNone);
}

TEST(memcard_port_from_channel) {
  reset(NULL);
  card_paths[1] = CARD;
  RunMemCardInfoOp(0x00);
  CHECK_INT(sync_result(), McFuncExist << 8 | McErrCardNotExist);
  RunMemCardInfoOp(0x10);
  CHECK_INT(sync_result(), McFuncExist << 8 | McErrNone);
}

/* --- format -------------------------------------------------------------- */

TEST(memcard_missing_image_is_formatted) {
  reset(CARD);
  MemCardLoad(0x00);
  CHECK_INT(sync_result(), McFuncAccept << 8 | McErrNone);
  /* not written until something changes */
  CHECK_INT(file_length(CARD), -1);

  Card *c = &cards[0];
  CHECK(c->data[0] == 'M' && c->data[1] == 'C');
  CHECK(frame(c, 0, 63)[0] == 'M' && frame(c, 0, 63)[1] == 'C');
  for (int n = 0; n < 64; n++) {
    CHECK(frame_checksum_ok(frame(c, 0, n)));
  }
  for (int b = 1; b < N_BLOCKS; b++) {
    unsigned char *f = frame(c, 0, b);

    CHECK_INT(get32(f), DIR_FREE);
    CHECK_INT(f[8] | f[9] << 8, DIR_NO_NEXT);
  }
  CHECK_INT(get32(frame(c, 0, N_BLOCKS)), 0xFFFFFFFF); /* no broken sectors */
}

TEST(memcard_unformatted_image) {
  unsigned char buf[1];

  reset(CARD);
  write_image(CARD, CARD_SIZE, 0);
  MemCardLoad(0x00);
  CHECK_INT(sync_result(), McFuncAccept << 8 | McErrNotFormat);
  CHECK_INT(MemCardCreateFile(0x00, "BASLUS", 1), McErrNotFormat);
  MemCardRead(0x00, "BASLUS", buf, 0, 1);
  CHECK_INT(sync_result(), McFuncReadFile << 8 | McErrNotFormat);

  CHECK_INT(MemCardFormat(0x00), McErrNone);
  MemCardLoad(0x00);
  CHECK_INT(sync_result(), McFuncAccept << 8 | McErrNone);
  CHECK_INT(file_length(CARD), CARD_SIZE);
}

TEST(memcard_short_image_is_no_card) {
  reset(CARD);
  write_image(CARD, 1000, 0);
  RunMemCardInfoOp(0x00);
  CHECK_INT(sync_result(), McFuncExist << 8 | McErrCardNotExist);
}

/* --- files --------------------------------------------------------------- */

TEST(memcard_create_file) {
  reset(CARD);
  CHECK_INT(MemCardCreateFile(0x00, "BASCUS-94228", 1), McErrNone);
  CHECK_INT(file_length(CARD), CARD_SIZE);

  unsigned char *f = frame(&cards[0], 0, 1);
  CHECK_INT(get32(f), DIR_FIRST);
  CHECK_INT(get32(f + 4), BLOCK);
  CHECK_INT(f[8] | f[9] << 8, DIR_NO_NEXT);
  CHECK_STR((const char *)f + 0x0A, "BASCUS-94228");
  CHECK(frame_checksum_ok(f));
}

TEST(memcard_create_file_errors) {
  reset(CARD);
  CHECK_INT(MemCardCreateFile(0x00, "A", 1), McErrNone);
  CHECK_INT(MemCardCreateFile(0x00, "A", 1), McErrAlreadyExist);
  CHECK_INT(MemCardCreateFile(0x00, "B", 0), McErrBlockFull);
  CHECK_INT(MemCardCreateFile(0x00, "B", 15), McErrBlockFull); /* 14 free */
  CHECK_INT(MemCardCreateFile(0x00, "B", 14), McErrNone);
  CHECK_INT(MemCardCreateFile(0x00, "C", 1), McErrBlockFull);
}

TEST(memcard_multi_block_chain) {
  reset(CARD);
  CHECK_INT(MemCardCreateFile(0x00, "ONE", 1), McErrNone);   /* block 1 */
  CHECK_INT(MemCardCreateFile(0x00, "THREE", 3), McErrNone); /* blocks 2-4 */

  Card *c = &cards[0];
  CHECK_INT(find_file(c, "ONE"), 1);
  CHECK_INT(find_file(c, "THREE"), 2);
  CHECK_INT(find_file(c, "TWO"), 0);
  CHECK_INT(get32(frame(c, 0, 2)), DIR_FIRST);
  CHECK_INT(get32(frame(c, 0, 2) + 4), 3 * BLOCK);
  CHECK_INT(get32(frame(c, 0, 3)), DIR_MIDDLE);
  CHECK_INT(get32(frame(c, 0, 4)), DIR_LAST);
  /* next pointers hold the block number - 1 */
  CHECK_INT(frame(c, 0, 2)[8], 2);
  CHECK_INT(frame(c, 0, 3)[8], 3);
  CHECK_INT(frame(c, 0, 4)[8] | frame(c, 0, 4)[9] << 8, DIR_NO_NEXT);
}

TEST(memcard_write_read_across_blocks) {
  static unsigned char out[3 * 8192], in[3 * 8192];

  reset(CARD);
  CHECK_INT(MemCardCreateFile(0x00, "ONE", 1), McErrNone);
  CHECK_INT(MemCardCreateFile(0x00, "THREE", 3), McErrNone);
  for (int i = 0; i < (int)sizeof(out); i++) {
    out[i] = (unsigned char)(i * 7 + 1);
  }
  MemCardWrite(0x00, "THREE", out, 0, sizeof(out));
  CHECK_INT(sync_result(), McFuncWriteFile << 8 | McErrNone);

  /* the image on disk holds it */
  reload();
  MemCardRead(0x00, "THREE", in, 0, sizeof(in));
  CHECK_INT(sync_result(), McFuncReadFile << 8 | McErrNone);
  CHECK(memcmp(in, out, sizeof(in)) == 0);

  /* an offset read spanning the block 2 / 3 boundary */
  memset(in, 0, sizeof(in));
  MemCardRead(0x00, "THREE", in, BLOCK - 100, 300);
  CHECK_INT(sync_result(), McFuncReadFile << 8 | McErrNone);
  CHECK(memcmp(in, out + BLOCK - 100, 300) == 0);

  /* the other file is untouched */
  MemCardRead(0x00, "ONE", in, 0, BLOCK);
  CHECK_INT(sync_result(), McFuncReadFile << 8 | McErrNone);
  for (int i = 0; i < BLOCK; i++) {
    CHECK_INT(in[i], 0);
  }
}

TEST(memcard_partial_write_keeps_rest) {
  unsigned char buf[16];

  reset(CARD);
  CHECK_INT(MemCardCreateFile(0x00, "F", 1), McErrNone);
  MemCardWrite(0x00, "F", "abcdefgh", 0, 8);
  CHECK_INT(sync_result(), McFuncWriteFile << 8 | McErrNone);
  MemCardWrite(0x00, "F", "XY", 3, 2);
  CHECK_INT(sync_result(), McFuncWriteFile << 8 | McErrNone);
  memset(buf, 0, sizeof(buf));
  MemCardRead(0x00, "F", buf, 0, 8);
  CHECK_INT(sync_result(), McFuncReadFile << 8 | McErrNone);
  CHECK_STR((const char *)buf, "abcXYfgh");
}

TEST(memcard_file_op_errors) {
  unsigned char buf[16];

  reset(CARD);
  CHECK_INT(MemCardCreateFile(0x00, "F", 1), McErrNone);
  MemCardRead(0x00, "G", buf, 0, 1);
  CHECK_INT(sync_result(), McFuncReadFile << 8 | McErrFileNotExist);
  MemCardRead(0x00, "F", buf, BLOCK - 4, 8); /* past the file's size */
  CHECK_INT(sync_result(), McFuncReadFile << 8 | McErrCardInvalid);
  MemCardWrite(0x00, "F", buf, -1, 1);
  CHECK_INT(sync_result(), McFuncWriteFile << 8 | McErrCardInvalid);
  MemCardRead(0x00, "F", buf, 0, -1);
  CHECK_INT(sync_result(), McFuncReadFile << 8 | McErrCardInvalid);
}

TEST(memcard_format_erases_files) {
  reset(CARD);
  CHECK_INT(MemCardCreateFile(0x00, "F", 2), McErrNone);
  CHECK_INT(MemCardFormat(0x00), McErrNone);
  CHECK_INT(find_file(&cards[0], "F"), 0);
  reload();
  CHECK_INT(MemCardCreateFile(0x00, "F", 15), McErrNone);
}
