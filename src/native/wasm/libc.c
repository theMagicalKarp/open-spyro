/* src/native/wasm/libc.c — the freestanding C library of the native wasm32
 * module.
 *
 * The module is position-independent (game globals resolve to PS1 addresses
 * through GOT imports, native/flags.sh), and wasi-libc's static libc.a is not
 * PIC, so it cannot be linked in. The game and PsyZ core need little of libc,
 * so it is implemented here: string and memory functions, a printf family,
 * a minimal sscanf, and stdio over host imports (host/imports.c). The
 * wasi-sysroot headers still provide the declarations. */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>

#include "host_imports.h"

/* --- memory / string ------------------------------------------------------ */

void *memcpy(void *restrict dst, const void *restrict src, size_t n) {
  unsigned char *d = dst;
  const unsigned char *s = src;

  while (n--) {
    *d++ = *s++;
  }
  return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
  unsigned char *d = dst;
  const unsigned char *s = src;

  if (d < s) {
    while (n--) {
      *d++ = *s++;
    }
  } else {
    while (n--) {
      d[n] = s[n];
    }
  }
  return dst;
}

void *memset(void *dst, int c, size_t n) {
  unsigned char *d = dst;

  while (n--) {
    *d++ = (unsigned char)c;
  }
  return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
  const unsigned char *x = a;
  const unsigned char *y = b;

  for (; n; n--, x++, y++) {
    if (*x != *y) {
      return *x - *y;
    }
  }
  return 0;
}

size_t strlen(const char *s) {
  const char *p = s;

  while (*p) {
    p++;
  }
  return (size_t)(p - s);
}

/* clang's loop-idiom pass turns a count-to-zero loop over ints into wcslen
   (wchar_t is 32-bit here); game C has such loops. */
size_t wcslen(const wchar_t *s) {
  const wchar_t *p = s;

  while (*p) {
    p++;
  }
  return (size_t)(p - s);
}

int strcmp(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
  for (; n; n--, a++, b++) {
    if (*a != *b || !*a) {
      return (unsigned char)*a - (unsigned char)*b;
    }
  }
  return 0;
}

char *strcpy(char *restrict dst, const char *restrict src) {
  char *d = dst;

  while ((*d++ = *src++)) {
  }
  return dst;
}

char *strncpy(char *restrict dst, const char *restrict src, size_t n) {
  size_t i = 0;

  for (; i < n && src[i]; i++) {
    dst[i] = src[i];
  }
  for (; i < n; i++) {
    dst[i] = '\0';
  }
  return dst;
}

char *strcat(char *restrict dst, const char *restrict src) {
  strcpy(dst + strlen(dst), src);
  return dst;
}

char *strchr(const char *s, int c) {
  for (;; s++) {
    if (*s == (char)c) {
      return (char *)s;
    }
    if (!*s) {
      return NULL;
    }
  }
}

char *strrchr(const char *s, int c) {
  const char *last = NULL;

  for (;; s++) {
    if (*s == (char)c) {
      last = s;
    }
    if (!*s) {
      return (char *)last;
    }
  }
}

/* --- printf family -------------------------------------------------------- */

typedef struct {
  char *buf;
  size_t cap;
  size_t len; /* characters produced (may exceed cap) */
} Out;

static void put(Out *o, char c) {
  if (o->len + 1 < o->cap) {
    o->buf[o->len] = c;
  }
  o->len++;
}

static void pad(Out *o, char c, int n) {
  while (n-- > 0) {
    put(o, c);
  }
}

enum { F_LEFT = 1, F_PLUS = 2, F_SPACE = 4, F_ALT = 8, F_ZERO = 16 };

static void emit_number(Out *o, uintmax_t v, int neg, unsigned base, int upper,
                        int flags, int width, int prec) {
  static const char lo[] = "0123456789abcdef";
  static const char hi[] = "0123456789ABCDEF";
  const char *digits = upper ? hi : lo;
  char tmp[24];
  int n = 0;
  int zeros;
  int total;
  char sign = 0;
  const char *prefix = "";

  while (v) {
    tmp[n++] = digits[v % base];
    v /= base;
  }
  if (prec < 0) {
    prec = 1;
  } else {
    flags &= ~F_ZERO;
  }
  zeros = prec > n ? prec - n : 0;
  if (neg) {
    sign = '-';
  } else if (flags & F_PLUS) {
    sign = '+';
  } else if (flags & F_SPACE) {
    sign = ' ';
  }
  if ((flags & F_ALT) && base == 16 && n) {
    prefix = upper ? "0X" : "0x";
  } else if ((flags & F_ALT) && base == 8 && zeros == 0) {
    zeros = 1;
  }
  total = n + zeros + (sign != 0) + (int)strlen(prefix);
  if (!(flags & (F_LEFT | F_ZERO))) {
    pad(o, ' ', width - total);
  }
  if (sign) {
    put(o, sign);
  }
  for (; *prefix; prefix++) {
    put(o, *prefix);
  }
  if (flags & F_ZERO) {
    pad(o, '0', width - total);
  }
  pad(o, '0', zeros);
  while (n) {
    put(o, tmp[--n]);
  }
  if (flags & F_LEFT) {
    pad(o, ' ', width - total);
  }
}

int vsnprintf(char *restrict buf, size_t cap, const char *restrict fmt,
              va_list ap) {
  Out o = {buf, cap, 0};

  for (; *fmt; fmt++) {
    int flags = 0;
    int width = 0;
    int prec = -1;
    int len = 0; /* -2 hh, -1 h, 0 int, 1 l, 2 ll / j / z / t */
    uintmax_t u;
    intmax_t s;

    if (*fmt != '%') {
      put(&o, *fmt);
      continue;
    }
    fmt++;
    for (;; fmt++) {
      if (*fmt == '-') {
        flags |= F_LEFT;
      } else if (*fmt == '+') {
        flags |= F_PLUS;
      } else if (*fmt == ' ') {
        flags |= F_SPACE;
      } else if (*fmt == '#') {
        flags |= F_ALT;
      } else if (*fmt == '0') {
        flags |= F_ZERO;
      } else {
        break;
      }
    }
    if (*fmt == '*') {
      width = va_arg(ap, int);
      if (width < 0) {
        flags |= F_LEFT;
        width = -width;
      }
      fmt++;
    } else {
      for (; *fmt >= '0' && *fmt <= '9'; fmt++) {
        width = width * 10 + (*fmt - '0');
      }
    }
    if (*fmt == '.') {
      fmt++;
      prec = 0;
      if (*fmt == '*') {
        prec = va_arg(ap, int);
        fmt++;
      } else {
        for (; *fmt >= '0' && *fmt <= '9'; fmt++) {
          prec = prec * 10 + (*fmt - '0');
        }
      }
    }
    for (;; fmt++) {
      if (*fmt == 'h') {
        len--;
      } else if (*fmt == 'l') {
        len++;
      } else if (*fmt == 'j' || *fmt == 'z' || *fmt == 't') {
        len = 2;
      } else {
        break;
      }
    }
    switch (*fmt) {
    case 'd':
    case 'i':
      s = len >= 2   ? va_arg(ap, long long)
          : len == 1 ? va_arg(ap, long)
                     : va_arg(ap, int);
      if (len == -1) {
        s = (short)s;
      } else if (len <= -2) {
        s = (signed char)s;
      }
      emit_number(&o, s < 0 ? (uintmax_t)-s : (uintmax_t)s, s < 0, 10, 0, flags,
                  width, prec);
      break;
    case 'u':
    case 'x':
    case 'X':
    case 'o':
      u = len >= 2   ? va_arg(ap, unsigned long long)
          : len == 1 ? va_arg(ap, unsigned long)
                     : va_arg(ap, unsigned int);
      if (len == -1) {
        u = (unsigned short)u;
      } else if (len <= -2) {
        u = (unsigned char)u;
      }
      emit_number(&o, u, 0,
                  *fmt == 'o'   ? 8
                  : *fmt == 'u' ? 10
                                : 16,
                  *fmt == 'X', flags, width, prec);
      break;
    case 'p':
      emit_number(&o, (uintptr_t)va_arg(ap, void *), 0, 16, 0, F_ALT, width,
                  -1);
      break;
    case 'c':
      if (!(flags & F_LEFT)) {
        pad(&o, ' ', width - 1);
      }
      put(&o, (char)va_arg(ap, int));
      if (flags & F_LEFT) {
        pad(&o, ' ', width - 1);
      }
      break;
    case 's': {
      const char *str = va_arg(ap, const char *);
      int n = 0;

      if (str == NULL) {
        str = "(null)";
      }
      while (str[n] && (prec < 0 || n < prec)) {
        n++;
      }
      if (!(flags & F_LEFT)) {
        pad(&o, ' ', width - n);
      }
      for (int i = 0; i < n; i++) {
        put(&o, str[i]);
      }
      if (flags & F_LEFT) {
        pad(&o, ' ', width - n);
      }
      break;
    }
    case '%':
      put(&o, '%');
      break;
    case '\0':
      fmt--;
      break;
    default: /* floats and the rest are not used by the game or PsyZ core */
      put(&o, '%');
      put(&o, *fmt);
      break;
    }
  }
  if (cap) {
    buf[o.len < cap ? o.len : cap - 1] = '\0';
  }
  return (int)o.len;
}

int snprintf(char *restrict buf, size_t cap, const char *restrict fmt, ...) {
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = vsnprintf(buf, cap, fmt, ap);
  va_end(ap);
  return n;
}

int vsprintf(char *restrict buf, const char *restrict fmt, va_list ap) {
  return vsnprintf(buf, SIZE_MAX, fmt, ap);
}

int sprintf(char *restrict buf, const char *restrict fmt, ...) {
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = vsnprintf(buf, SIZE_MAX, fmt, ap);
  va_end(ap);
  return n;
}

int vprintf(const char *restrict fmt, va_list ap) {
  char buf[1024];
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);

  host_write(1, buf, n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1);
  return n;
}

int printf(const char *restrict fmt, ...) {
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = vprintf(fmt, ap);
  va_end(ap);
  return n;
}

int puts(const char *s) {
  host_write(1, s, (int)strlen(s));
  host_write(1, "\n", 1);
  return 0;
}

/* --- sscanf: %d, %s and literal matching (all PsyZ's cue parser uses) ---- */

static int is_space(int c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

int vsscanf(const char *restrict str, const char *restrict fmt, va_list ap) {
  int assigned = 0;

  for (; *fmt; fmt++) {
    if (is_space((unsigned char)*fmt)) {
      while (is_space((unsigned char)*str)) {
        str++;
      }
      continue;
    }
    if (*fmt != '%') {
      if (*str != *fmt) {
        return assigned;
      }
      str++;
      continue;
    }
    fmt++;
    int width = 0;
    for (; *fmt >= '0' && *fmt <= '9'; fmt++) {
      width = width * 10 + (*fmt - '0');
    }
    while (is_space((unsigned char)*str)) {
      str++;
    }
    if (*str == '\0') {
      return assigned ? assigned : EOF;
    }
    if (*fmt == 'd') {
      int neg = 0;
      int v = 0;

      if (*str == '-' || *str == '+') {
        neg = *str++ == '-';
      }
      if (*str < '0' || *str > '9') {
        return assigned;
      }
      for (; *str >= '0' && *str <= '9'; str++) {
        v = v * 10 + (*str - '0');
      }
      *va_arg(ap, int *) = neg ? -v : v;
      assigned++;
    } else if (*fmt == 's') {
      char *dst = va_arg(ap, char *);
      int n = 0;

      while (*str && !is_space((unsigned char)*str) && (!width || n < width)) {
        dst[n++] = *str++;
      }
      dst[n] = '\0';
      assigned++;
    } else {
      return assigned;
    }
  }
  return assigned;
}

int sscanf(const char *restrict str, const char *restrict fmt, ...) {
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = vsscanf(str, fmt, ap);
  va_end(ap);
  return n;
}

/* --- stdio over host file handles ----------------------------------------- */

/* A FILE * is the host handle + 1 (never NULL); the host owns the stream. */
#define HANDLE(fp) ((int)(intptr_t)(fp) - 1)

FILE *fopen(const char *restrict path, const char *restrict mode) {
  int h = host_fopen(path, mode);

  return h < 0 ? NULL : (FILE *)(intptr_t)(h + 1);
}

int fclose(FILE *fp) { return host_fclose(HANDLE(fp)); }

size_t fread(void *restrict dst, size_t size, size_t n, FILE *restrict fp) {
  if (size == 0 || n == 0) {
    return 0;
  }
  return (size_t)host_fread(dst, (int)(size * n), HANDLE(fp)) / size;
}

size_t fwrite(const void *restrict src, size_t size, size_t n,
              FILE *restrict fp) {
  if (size == 0 || n == 0) {
    return 0;
  }
  return (size_t)host_fwrite(src, (int)(size * n), HANDLE(fp)) / size;
}

int fseek(FILE *fp, long off, int whence) {
  return host_fseek(HANDLE(fp), off, whence);
}

long ftell(FILE *fp) { return host_ftell(HANDLE(fp)); }

char *fgets(char *restrict buf, int n, FILE *restrict fp) {
  int i = 0;

  while (i < n - 1) {
    char c;

    if (host_fread(&c, 1, HANDLE(fp)) != 1) {
      break;
    }
    buf[i++] = c;
    if (c == '\n') {
      break;
    }
  }
  if (i == 0) {
    return NULL;
  }
  buf[i] = '\0';
  return buf;
}

int stat(const char *restrict path, struct stat *restrict st) {
  int size = host_file_size(path);

  if (size < 0) {
    return -1;
  }
  memset(st, 0, sizeof(*st));
  st->st_size = size;
  st->st_mode = S_IFREG;
  return 0;
}

int mkdir(const char *path, mode_t mode) {
  (void)mode;
  return host_mkdir(path);
}

/* --- process -------------------------------------------------------------- */

void exit(int code) {
  host_exit(code);
  __builtin_unreachable();
}

void abort(void) {
  host_exit(134);
  __builtin_unreachable();
}

void __assert_fail(const char *expr, const char *file, int line,
                   const char *func) {
  printf("assertion failed: %s (%s:%d, %s)\n", expr, file, line, func);
  abort();
}
