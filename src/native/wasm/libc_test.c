/* src/native/wasm/libc_test.c — unit tests for libc.c (make native-test).
 *
 * libc.c defines libc's own names, so it is included here with each of them
 * renamed to w_<name>: the tests call the module's implementation while the
 * harness keeps the host's. The system headers are included first, so the
 * renames reach only libc.c's definitions and calls. The host imports it uses
 * are implemented over host stdio. */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>

#include "host_imports.h"
#include "test.h"

/* --- host imports -------------------------------------------------------- */

/* host_write output, until written() reads it back. */
static char written_buf[256];
static size_t written_len;

int host_write(int fd, const void *buf, int len) {
  (void)fd;
  if (written_len + (size_t)len < sizeof(written_buf)) {
    memcpy(written_buf + written_len, buf, (size_t)len);
    written_len += (size_t)len;
  }
  return len;
}

static const char *written(void) {
  written_buf[written_len] = '\0';
  written_len = 0;
  return written_buf;
}

void host_log(int level, const char *msg) { (void)level, (void)msg; }
_Noreturn void host_exit(int code) { exit(code); }
unsigned int host_now_us(void) { return 0; }

static FILE *host_files[8];

int host_fopen(const char *path, const char *mode) {
  for (int i = 0; i < 8; i++) {
    if (host_files[i] == NULL) {
      host_files[i] = fopen(path, mode);
      return host_files[i] ? i : -1;
    }
  }
  return -1;
}
int host_fclose(int h) {
  int r = fclose(host_files[h]);

  host_files[h] = NULL;
  return r;
}
int host_fread(void *dst, int len, int h) {
  return (int)fread(dst, 1, (size_t)len, host_files[h]);
}
int host_fwrite(const void *src, int len, int h) {
  return (int)fwrite(src, 1, (size_t)len, host_files[h]);
}
int host_fseek(int h, long off, int whence) {
  return fseek(host_files[h], off, whence);
}
long host_ftell(int h) { return ftell(host_files[h]); }
int host_file_size(const char *path) {
  struct stat st;

  return stat(path, &st) == 0 ? (int)st.st_size : -1;
}
int host_mkdir(const char *path) { return mkdir(path, 0755); }

/* --- the module's libc, renamed ------------------------------------------ */

#define W_(name) w_##name
#undef memcpy
#undef memmove
#undef memset
#undef strcpy
#undef strncpy
#undef strcat
#undef snprintf
#undef vsnprintf
#undef sprintf
#undef vsprintf
#define memcpy(...) W_(memcpy)(__VA_ARGS__)
#define memmove(...) W_(memmove)(__VA_ARGS__)
#define memset(...) W_(memset)(__VA_ARGS__)
#define memcmp(...) W_(memcmp)(__VA_ARGS__)
#define strlen(...) W_(strlen)(__VA_ARGS__)
#define wcslen(...) W_(wcslen)(__VA_ARGS__)
#define strcmp(...) W_(strcmp)(__VA_ARGS__)
#define strncmp(...) W_(strncmp)(__VA_ARGS__)
#define strcpy(...) W_(strcpy)(__VA_ARGS__)
#define strncpy(...) W_(strncpy)(__VA_ARGS__)
#define strcat(...) W_(strcat)(__VA_ARGS__)
#define strchr(...) W_(strchr)(__VA_ARGS__)
#define strrchr(...) W_(strrchr)(__VA_ARGS__)
#define vsnprintf(...) W_(vsnprintf)(__VA_ARGS__)
#define snprintf(...) W_(snprintf)(__VA_ARGS__)
#define vsprintf(...) W_(vsprintf)(__VA_ARGS__)
#define sprintf(...) W_(sprintf)(__VA_ARGS__)
#define vprintf(...) W_(vprintf)(__VA_ARGS__)
#define printf(...) W_(printf)(__VA_ARGS__)
#define puts(...) W_(puts)(__VA_ARGS__)
#define vsscanf(...) W_(vsscanf)(__VA_ARGS__)
#define sscanf(...) W_(sscanf)(__VA_ARGS__)
#define fopen(...) W_(fopen)(__VA_ARGS__)
#define fclose(...) W_(fclose)(__VA_ARGS__)
#define fread(...) W_(fread)(__VA_ARGS__)
#define fwrite(...) W_(fwrite)(__VA_ARGS__)
#define fseek(...) W_(fseek)(__VA_ARGS__)
#define ftell(...) W_(ftell)(__VA_ARGS__)
#define fgets(...) W_(fgets)(__VA_ARGS__)
#define stat(...) W_(stat)(__VA_ARGS__) /* struct stat is not followed by ( */
#define mkdir(...) W_(mkdir)(__VA_ARGS__)
#define exit(...) W_(exit)(__VA_ARGS__)
#define abort(...) W_(abort)(__VA_ARGS__)
#define __assert_fail(...) W_(__assert_fail)(__VA_ARGS__)

#include "libc.c"

/* --- printf family ------------------------------------------------------- */

static char out[64];

/* w_snprintf into `out`; returns it. */
static const char *fmt(const char *f, ...) {
  va_list ap;

  va_start(ap, f);
  w_vsnprintf(out, sizeof(out), f, ap);
  va_end(ap);
  return out;
}

TEST(libc_printf_ints) {
  CHECK_STR(fmt("%d %i", 42, -42), "42 -42");
  CHECK_STR(fmt("%u", -1), "4294967295");
  CHECK_STR(fmt("%x %X %o", 0xBEEF, 0xBEEF, 8), "beef BEEF 10");
  CHECK_STR(fmt("%d", 0), "0");
  CHECK_STR(fmt("%d", (int)0x80000000), "-2147483648");
}

TEST(libc_printf_lengths) {
  CHECK_STR(fmt("%hhd %hhu", 300, 300), "44 44");
  CHECK_STR(fmt("%hd %hx", 70000, 0x12345), "4464 2345");
  CHECK_STR(fmt("%ld", -5L), "-5");
  CHECK_STR(fmt("%lld", -1234567890123LL), "-1234567890123");
  CHECK_STR(fmt("%llx", 0x123456789ABCULL), "123456789abc");
  CHECK_STR(fmt("%zu", (size_t)7), "7");
}

TEST(libc_printf_flags_and_width) {
  CHECK_STR(fmt("[%5d]", 42), "[   42]");
  CHECK_STR(fmt("[%-5d]", 42), "[42   ]");
  CHECK_STR(fmt("[%05d]", -42), "[-0042]");
  CHECK_STR(fmt("[%+d %+d]", 3, -3), "[+3 -3]");
  CHECK_STR(fmt("[% d]", 3), "[ 3]");
  CHECK_STR(fmt("[%*d]", 4, 1), "[   1]");
  CHECK_STR(fmt("[%*d]", -4, 1), "[1   ]");
  CHECK_STR(fmt("[%08X]", 0xABC), "[00000ABC]");
}

TEST(libc_printf_precision) {
  CHECK_STR(fmt("[%.3d]", 7), "[007]");
  CHECK_STR(fmt("[%5.3d]", -7), "[ -007]");
  CHECK_STR(fmt("[%.0d]", 0), "[]");
  /* a precision turns off zero padding */
  CHECK_STR(fmt("[%06.2d]", 5), "[    05]");
}

TEST(libc_printf_alternate) {
  CHECK_STR(fmt("%#x %#X", 255, 255), "0xff 0XFF");
  CHECK_STR(fmt("%#x", 0), "0");
  CHECK_STR(fmt("%#o %#o", 8, 0), "010 0");
  CHECK_STR(fmt("%p", (void *)0x1234), "0x1234");
}

TEST(libc_printf_strings_and_chars) {
  CHECK_STR(fmt("%s|%.2s|%6s|%-6s|", "abc", "abc", "abc", "abc"),
            "abc|ab|   abc|abc   |");
  CHECK_STR(fmt("%.*s", 1, "xyz"), "x");
  CHECK_STR(fmt("%s", (char *)NULL), "(null)");
  CHECK_STR(fmt("%c%c|%3c|%-3c|", 'h', 'i', 'x', 'y'), "hi|  x|y  |");
  CHECK_STR(fmt("100%%"), "100%");
}

TEST(libc_printf_unsupported) {
  /* floats are not implemented: the conversion is copied, no arg read */
  CHECK_STR(fmt("%f %d", 9), "%f 9");
  /* a trailing % is dropped */
  CHECK_STR(fmt("50%"), "50");
}

TEST(libc_snprintf_truncates) {
  char buf[8];

  memset(buf, 'X', sizeof(buf));
  CHECK_INT(w_snprintf(buf, 5, "%s", "abcdefgh"), 8);
  CHECK_STR(buf, "abcd");
  CHECK_INT(buf[5], 'X');
  CHECK_INT(w_snprintf(buf, 0, "%d", 12345), 5);
  CHECK_INT(buf[0], 'a');
  CHECK_INT(w_snprintf(buf, 1, "xyz"), 3);
  CHECK_STR(buf, "");
}

TEST(libc_sprintf) {
  char buf[32];

  CHECK_INT(w_sprintf(buf, "%s-%02d", "lvl", 3), 6);
  CHECK_STR(buf, "lvl-03");
}

TEST(libc_printf_writes_stdout) {
  written();
  CHECK_INT(w_printf("x=%d\n", 5), 4);
  CHECK_STR(written(), "x=5\n");
  w_puts("line");
  CHECK_STR(written(), "line\n");
}

/* --- sscanf -------------------------------------------------------------- */

TEST(libc_sscanf_ints) {
  int a = 0, b = 0;

  CHECK_INT(w_sscanf("12 -3", "%d %d", &a, &b), 2);
  CHECK_INT(a, 12);
  CHECK_INT(b, -3);
  CHECK_INT(w_sscanf("  +7", "%d", &a), 1);
  CHECK_INT(a, 7);
  CHECK_INT(w_sscanf("x", "%d", &a), 0);
  CHECK_INT(w_sscanf("-", "%d", &a), 0);
}

TEST(libc_sscanf_strings) {
  char s[16];
  int n = 0;

  CHECK_INT(w_sscanf("TRACK 01 MODE2/2352", "TRACK %d %s", &n, s), 2);
  CHECK_INT(n, 1);
  CHECK_STR(s, "MODE2/2352");
  CHECK_INT(w_sscanf("abcdef", "%3s", s), 1);
  CHECK_STR(s, "abc");
}

TEST(libc_sscanf_literals_and_end) {
  int a = 0, b = 0;

  CHECK_INT(w_sscanf("1:2", "%d:%d", &a, &b), 2);
  CHECK_INT(w_sscanf("1;2", "%d:%d", &a, &b), 1);
  CHECK_INT(w_sscanf("", "%d", &a), EOF);
  CHECK_INT(w_sscanf("   ", "%d", &a), EOF);
  CHECK_INT(w_sscanf("5", "%d %d", &a, &b), 1);
  /* unsupported conversions stop the scan */
  CHECK_INT(w_sscanf("5", "%x", &a), 0);
}

/* --- memory / string ----------------------------------------------------- */

TEST(libc_memmove_overlap) {
  char a[] = "abcdefgh";
  char b[] = "abcdefgh";

  w_memmove(a + 2, a, 5);
  CHECK_STR(a, "ababcdeh");
  w_memmove(b, b + 2, 5);
  CHECK_STR(b, "cdefgfgh");
}

TEST(libc_memcmp_and_memset) {
  unsigned char a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 0xFF, 0};

  CHECK(w_memcmp(a, b, 4) < 0);
  CHECK(w_memcmp(b, a, 4) > 0);
  CHECK_INT(w_memcmp(a, b, 2), 0);
  CHECK_INT(w_memcmp(a, b, 0), 0);
  w_memset(a, 0x1FF, 3);
  CHECK(a[0] == 0xFF && a[2] == 0xFF && a[3] == 4);
}

TEST(libc_strcmp_family) {
  CHECK_INT(w_strcmp("abc", "abc"), 0);
  CHECK(w_strcmp("abc", "abd") < 0);
  CHECK(w_strcmp("ab", "abc") < 0);
  CHECK(w_strcmp("\xFF", "a") > 0); /* compared as unsigned char */
  CHECK_INT(w_strncmp("abcx", "abcy", 3), 0);
  CHECK(w_strncmp("abcx", "abcy", 4) < 0);
  CHECK_INT(w_strncmp("ab", "ab", 10), 0);
  CHECK_INT(w_strncmp("a", "b", 0), 0);
}

TEST(libc_strcpy_family) {
  char buf[8];

  w_memset(buf, 'X', sizeof(buf));
  w_strncpy(buf, "ab", 5);
  CHECK(w_memcmp(buf, "ab\0\0\0XXX", 8) == 0); /* zero-padded to n */
  w_strncpy(buf, "abcdefgh", 3);
  CHECK(w_memcmp(buf, "abc\0\0XXX", 8) == 0); /* not terminated */
  w_strcpy(buf, "hi");
  w_strcat(buf, " you");
  CHECK_STR(buf, "hi you");
  CHECK_INT(w_strlen(buf), 6);
  CHECK_INT(w_strlen(""), 0);
}

TEST(libc_strchr_family) {
  const char *s = "a/b/c";

  CHECK(w_strchr(s, '/') == s + 1);
  CHECK(w_strrchr(s, '/') == s + 3);
  CHECK(w_strchr(s, 'z') == NULL);
  CHECK(w_strrchr(s, 'z') == NULL);
  CHECK(w_strchr(s, '\0') == s + 5);
  CHECK(w_strrchr(s, '\0') == s + 5);
}

TEST(libc_wcslen) {
  CHECK_INT(w_wcslen(L"spyro"), 5);
  CHECK_INT(w_wcslen(L""), 0);
}

/* --- stdio --------------------------------------------------------------- */

#define TEXT TEST_TMPDIR "/libc_text.txt"

TEST(libc_stdio_roundtrip) {
  FILE *fp = w_fopen(TEXT, "wb");
  char buf[16];
  struct stat st;

  CHECK(fp != NULL);
  CHECK_INT(w_fwrite("one\ntwo\nend", 1, 11, fp), 11);
  CHECK_INT(w_fclose(fp), 0);

  CHECK_INT(w_stat(TEXT, &st), 0);
  CHECK_INT(st.st_size, 11);
  CHECK(S_ISREG(st.st_mode));

  fp = w_fopen(TEXT, "rb");
  CHECK(fp != NULL);
  CHECK_STR(w_fgets(buf, sizeof(buf), fp), "one\n");
  CHECK_STR(w_fgets(buf, sizeof(buf), fp), "two\n");
  CHECK_INT(w_ftell(fp), 8);
  CHECK_STR(w_fgets(buf, sizeof(buf), fp), "end");
  CHECK(w_fgets(buf, sizeof(buf), fp) == NULL);

  CHECK_INT(w_fseek(fp, 2, SEEK_SET), 0);
  CHECK_STR(w_fgets(buf, 3, fp), "e\n"); /* n - 1 characters at most */
  CHECK_INT(w_fseek(fp, 0, SEEK_SET), 0);
  CHECK_INT(w_fread(buf, 4, 2, fp), 2); /* whole elements */
  CHECK_INT(w_fread(buf, 4, 2, fp), 0); /* 3 bytes left: no whole element */
  CHECK_INT(w_fread(buf, 0, 2, fp), 0);
  w_fclose(fp);
}

TEST(libc_stdio_missing) {
  struct stat st;

  CHECK(w_fopen(TEST_TMPDIR "/libc_missing.txt", "rb") == NULL);
  CHECK_INT(w_stat(TEST_TMPDIR "/libc_missing.txt", &st), -1);
}
