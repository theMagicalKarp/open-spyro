#include "globals.h"

extern void WriteChar(char c);
extern char *memchr(char *s, int c, int n);
extern int strlen(char *s);

extern unsigned char D_80074D15[]; /* _ctype_ + 1 */
extern char D_800119A8[];          /* "0123456789abcdef" */
extern char D_800119BC[];          /* "(null)" */
extern char D_800119C4[];          /* "0123456789ABCDEF" */

#define BUF 40

#define LONGINT 0x01   /* long integer */
#define LONGDBL 0x02   /* long double */
#define SHORTINT 0x04  /* short integer */
#define ALT 0x08       /* alternate form */
#define LADJUST 0x10   /* left adjustment */
#define ZEROPAD 0x20   /* zero (as opposed to blank) pad */
#define HEXPREFIX 0x40 /* add 0x or 0X prefix */

#define isascii(c) ((unsigned)(c) < 0x80)
#define isdigit(c) (D_80074D15[c] & 4)
#define todigit(c) ((c) - '0')

#define va_arg(list, mode) ((mode *)(list += sizeof(mode)))[-1]

#define ARG()                                                                  \
  _ulong = flags & LONGINT    ? va_arg(argp, long)                             \
           : flags & SHORTINT ? (short)va_arg(argp, int)                       \
                              : va_arg(argp, int);

/* Formatted output core for the kernel-console printf (0x800627d8, 1672
   bytes): the 4.3BSD-Reno vfprintf state machine without the floating-point
   conversions, emitting each character through WriteChar. Returns the number
   of characters produced by conversions (plain format text is written but,
   as in the original, not counted). */
int FormatAndWrite(void *fp, unsigned char *fmt0, char *argp) {
  unsigned char *fmt;
  int ch;
  int cnt;
  int n;
  char *t;
  unsigned long _ulong;
  int base;
  int flags;
  int dprec;
  int fieldsz;
  int fpprec;
  int prec;
  int realsz;
  int size;
  int width;
  char sign;
  char *digs;
  char buf[BUF];

  if (fmt0 == 0) {
    return 0;
  }
  fmt = fmt0;
  digs = D_800119A8;
  for (cnt = 0;; ++fmt) {
    if ((ch = *fmt) == 0) {
      return cnt;
    }
    if (ch != '%') {
      WriteChar(ch);
      continue;
    }
    flags = 0;
    dprec = 0;
    fpprec = 0;
    width = 0;
    prec = -1;
    sign = '\0';

  rflag:
    switch (*++fmt) {
    case '\0':
      return cnt;
    case ' ':
      if (!sign) {
        sign = ' ';
      }
      goto rflag;
    case '#':
      flags |= ALT;
      goto rflag;
    case '*':
      if ((width = va_arg(argp, int)) >= 0) {
        goto rflag;
      }
      width = -width;
      /* FALLTHROUGH */
    case '-':
      flags |= LADJUST;
      goto rflag;
    case '+':
      sign = '+';
      goto rflag;
    case '.':
      if (*++fmt == '*') {
        n = va_arg(argp, int);
      } else {
        n = 0;
        while (isascii(*fmt) && isdigit(*fmt)) {
          n = 10 * n + todigit(*fmt++);
        }
        --fmt;
      }
      prec = n < 0 ? -1 : n;
      goto rflag;
    case '0':
      flags |= ZEROPAD;
      goto rflag;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      n = 0;
      do {
        n = 10 * n + todigit(*fmt);
      } while (isascii(*++fmt) && isdigit(*fmt));
      width = n;
      --fmt;
      goto rflag;
    case 'L':
      flags |= LONGDBL;
      goto rflag;
    case 'h':
      flags |= SHORTINT;
      goto rflag;
    case 'l':
      flags |= LONGINT;
      goto rflag;
    case 'c':
      *(t = buf) = va_arg(argp, int);
      size = 1;
      sign = '\0';
      goto pforw;
    case 'D':
      flags |= LONGINT;
      /* FALLTHROUGH */
    case 'd':
    case 'i':
      ARG();
      if ((long)_ulong < 0) {
        _ulong = -_ulong;
        sign = '-';
      }
      base = 10;
      goto number;
    case 'n':
      if (flags & LONGINT) {
        *va_arg(argp, long *) = cnt;
      } else if (flags & SHORTINT) {
        *va_arg(argp, short *) = cnt;
      } else {
        *va_arg(argp, int *) = cnt;
      }
      break;
    case 'O':
      flags |= LONGINT;
      /* FALLTHROUGH */
    case 'o':
      ARG();
      base = 8;
      goto nosign;
    case 'p':
      _ulong = (unsigned long)va_arg(argp, void *);
      base = 16;
      goto nosign;
    case 's':
      if (!(t = va_arg(argp, char *))) {
        t = D_800119BC;
      }
      if (prec >= 0) {
        char *p;

        if ((p = memchr(t, 0, prec)) != 0) {
          size = p - t;
          if (size > prec) {
            size = prec;
          }
        } else {
          size = prec;
        }
      } else {
        size = strlen(t);
      }
      sign = '\0';
      goto pforw;
    case 'U':
      flags |= LONGINT;
      /* FALLTHROUGH */
    case 'u':
      ARG();
      base = 10;
      goto nosign;
    case 'X':
      digs = D_800119C4;
      /* FALLTHROUGH */
    case 'x':
      ARG();
      base = 16;
      if (flags & ALT && _ulong != 0) {
        flags |= HEXPREFIX;
      }
    nosign:
      sign = '\0';
    number:
      if ((dprec = prec) >= 0) {
        flags &= ~ZEROPAD;
      }
      t = buf + BUF;
      if (_ulong != 0 || dprec != 0) {
        do {
          *--t = digs[_ulong % base];
          _ulong /= base;
        } while (_ulong);
        digs = D_800119A8;
        if (flags & ALT && base == 8 && *t != '0') {
          *--t = '0';
        }
      }
      size = buf + BUF - t;

    pforw:
      fieldsz = size + fpprec;
      if (sign) {
        fieldsz++;
      }
      if (flags & HEXPREFIX) {
        fieldsz += 2;
      }
      realsz = dprec > fieldsz ? dprec : fieldsz;

      if ((flags & (LADJUST | ZEROPAD)) == 0 && width) {
        for (n = realsz; n < width; n++) {
          WriteChar(' ');
        }
      }
      if (sign) {
        WriteChar(sign);
      }
      if (flags & HEXPREFIX) {
        WriteChar('0');
        WriteChar(*fmt);
      }
      if ((flags & (LADJUST | ZEROPAD)) == ZEROPAD) {
        for (n = realsz; n < width; n++) {
          WriteChar('0');
        }
      }
      for (n = fieldsz; n < dprec; n++) {
        WriteChar('0');
      }
      for (n = size; --n >= 0;) {
        WriteChar(*t++);
      }
      while (--fpprec >= 0) {
        WriteChar('0');
      }
      if (flags & LADJUST) {
        for (n = realsz; n < width; n++) {
          WriteChar(' ');
        }
      }
      cnt += width > realsz ? width : realsz;
      break;
    default:
      cnt++;
      WriteChar(*fmt);
      break;
    }
  }
}
