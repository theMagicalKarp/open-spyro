/* src/native/host/host_io_test.c — unit tests for host_io.c (make
 * native-test). */

#include <stdio.h>
#include <string.h>

#include "host_io.h"
#include "test.h"

#define FILE_A TEST_TMPDIR "/host_io_a.txt"

static void make_file(const char *path, const char *text) {
  FILE *f = fopen(path, "wb");

  fputs(text, f);
  fclose(f);
}

/* --- file handles -------------------------------------------------------- */

/* A failed CHECK ends its test early, so each handle test starts by closing
 * whatever an earlier one left open (closing a free handle is a no-op). */
static void close_all(void) {
  for (unsigned i = 0; i < HOST_IO_MAX_FILES; i++) {
    host_io_close(i);
  }
}

TEST(host_io_open_read_close) {
  char buf[8] = {0};
  int h;

  close_all();
  make_file(FILE_A, "spyro");
  h = host_io_open(FILE_A, "rb");
  CHECK(h >= 0);
  CHECK(host_io_file((unsigned)h) != NULL);
  CHECK_INT(fread(buf, 1, sizeof(buf), host_io_file((unsigned)h)), 5);
  CHECK_STR(buf, "spyro");
  CHECK_INT(host_io_close((unsigned)h), 0);
  CHECK(host_io_file((unsigned)h) == NULL);
}

TEST(host_io_bad_handles) {
  close_all();
  CHECK(host_io_file(HOST_IO_MAX_FILES) == NULL);
  CHECK(host_io_file((unsigned)-1) == NULL);
  CHECK_INT(host_io_close(HOST_IO_MAX_FILES), -1);
  CHECK_INT(host_io_close((unsigned)-1), -1);
  CHECK_INT(host_io_close(HOST_IO_MAX_FILES - 1), -1); /* not open */
}

TEST(host_io_missing_file_takes_no_handle) {
  int first, h;

  close_all();
  make_file(FILE_A, "x");
  CHECK_INT(host_io_open(TEST_TMPDIR "/host_io_missing.txt", "rb"), -1);
  first = host_io_open(FILE_A, "rb");
  CHECK(first >= 0);
  host_io_close((unsigned)first);
  CHECK_INT(host_io_open(TEST_TMPDIR "/host_io_missing.txt", "rb"), -1);
  h = host_io_open(FILE_A, "rb");
  CHECK_INT(h, first);
  host_io_close((unsigned)h);
}

TEST(host_io_table_full_and_reuse) {
  int h[HOST_IO_MAX_FILES];

  close_all();
  make_file(FILE_A, "x");
  for (int i = 0; i < HOST_IO_MAX_FILES; i++) {
    h[i] = host_io_open(FILE_A, "rb");
    CHECK_INT(h[i], i);
  }
  CHECK_INT(host_io_open(FILE_A, "rb"), -1);
  /* a freed handle is the next one handed out: the lowest free */
  CHECK_INT(host_io_close(5), 0);
  CHECK_INT(host_io_close(9), 0);
  CHECK_INT(host_io_open(FILE_A, "rb"), 5);
  CHECK_INT(host_io_open(FILE_A, "rb"), 9);
  for (int i = 0; i < HOST_IO_MAX_FILES; i++) {
    CHECK_INT(host_io_close((unsigned)i), 0);
  }
}

/* --- card path ----------------------------------------------------------- */

TEST(host_io_copy_path_fits) {
  char buf[6];

  memset(buf, 'X', sizeof(buf));
  CHECK_INT(host_io_copy_path("a.mcd", buf, sizeof(buf)), 1); /* exact fit */
  CHECK_STR(buf, "a.mcd");
  CHECK_INT(host_io_copy_path("", buf, 1), 1);
  CHECK_STR(buf, "");
}

TEST(host_io_copy_path_rejects) {
  char buf[5];

  memset(buf, 'X', sizeof(buf));
  CHECK_INT(host_io_copy_path("a.mcd", buf, sizeof(buf)), 0); /* no room */
  CHECK_INT(buf[0], 'X');
  CHECK_INT(host_io_copy_path(NULL, buf, sizeof(buf)), 0);
  CHECK_INT(host_io_copy_path("", buf, 0), 0);
}

/* --- log ----------------------------------------------------------------- */

static char log_text[128];

/* host_io_log into a temporary stream; returns what it wrote. */
static const char *log_line(unsigned level, int min_level, const char *msg) {
  FILE *f = tmpfile();
  size_t n;

  host_io_log(f, level, min_level, msg);
  rewind(f);
  n = fread(log_text, 1, sizeof(log_text) - 1, f);
  log_text[n] = '\0';
  fclose(f);
  return log_text;
}

TEST(host_io_log_levels) {
  CHECK_STR(log_line(0, 0, "dbg"), "[D]dbg\n");
  CHECK_STR(log_line(1, 1, "hi"), "[I]hi\n");
  CHECK_STR(log_line(2, 1, "careful"), "[W]careful\n");
  CHECK_STR(log_line(3, 3, "bad"), "[E]bad\n");
}

TEST(host_io_log_filters) {
  CHECK_STR(log_line(0, 1, "dbg"), "");  /* below the level */
  CHECK_STR(log_line(2, 3, "warn"), ""); /* below the level */
  CHECK_STR(log_line(4, 0, "huh"), "");  /* not a level */
  CHECK_STR(log_line((unsigned)-1, 0, "neg"), "");
}
