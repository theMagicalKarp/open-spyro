#include "globals.h"

extern int ArcTan2(int y, int x, int high_precision);
extern int LookupSine(unsigned int angle_12_4);
extern int LookupCosine(unsigned int angle_12_4);
extern void ZeroVector(int *vec);
extern void AddVector(int *dst, int *a, int *b);
extern void SubtractVector(int *dst, int *a, int *b);
extern void ScaleVector3Sat(int *dst, int *src, int scale);
extern void LShiftVector3(int *vec, unsigned int bits);
extern void RShiftVector3(int *vec, unsigned int bits);
extern int strlen(char *s);

extern int g_nSpyroWorldPosY; /* g_anSpyroWorldPos[1] as its own symbol */

/* The per-letter banner record hung off the actor's +0x00 pointer. */
struct NameRec {
  void *owner; /* 0x00 */
  short count; /* 0x04 */
  short index; /* 0x06 */
};

struct Actor {
  struct NameRec *rec; /* 0x00 (the sign actor reads [0] as its level id) */
  char pad04[8];
  int pos[3]; /* 0x0c */
  char pad18[0x1e];
  short kind;         /* 0x36 */
  short yaw16;        /* 0x38 */
  char pad3a[0xc];    /* 0x3a */
  unsigned char yaw;  /* 0x46 */
  char pad47;         /* 0x47 */
  unsigned char skin; /* 0x48 */
  unsigned char slot; /* 0x49 */
  char pad4a[5];      /* 0x4a */
  unsigned char mode; /* 0x4f */
};

/* The banner record pointer is re-read before every field store (the original
   never proves the letter actor's +0 stays put across them). */
#define REC(p) (*(struct NameRec *volatile *)&(p)->rec)

/* Spells a name out in floating letter actors around `sign`. With `named` set
   the text is the level name for the sign's own level id and the arc hangs off
   the sign's yaw (mirrored, and flipped through the origin, when the sign's
   +0x49 slot is non-zero); otherwise it is the balloonist's name laid out
   facing Spyro. Letters are spawned through g_pfnLevelOverlayInitHook ('A'-'Z'
   map to id 0x1AA.., anything else to 0x4C) and threaded onto a radial arc:
   each glyph sits one `spacing` step further round the ring, bent outward by
   `bend` and lifted by the cosine of its position ('L' actors get an extra
   0x100 of height). Spaces advance the cursor without spawning.
   (0x8003c358, 908 bytes.) */
void func_8003C358(struct Actor *sign, int named) {
  /* Four 16-byte (VECTOR-shaped) slots at sp+0x10/0x20/0x30/0x40, plus 8 bytes
     of tail slack the original's frame reserves and never touches. */
  int radial[4];
  int spacing[4];
  int cursor[4];
  int bend[4];
  int scratch[2];
  char *name;
  int yaw;
  int a;
  int b;
  int ang;
  int step;
  int len;
  int n;
  int i;
  char *p;
  struct Actor *letter;
  int *pos;

  if (named != 0) {
    name = g_apLevelNameStrings[*(int *)sign->rec];
    yaw = sign->yaw;
  } else {
    name = g_apBalloonistNames[0];
    yaw = ArcTan2(g_anSpyroWorldPos[0] - sign->pos[0],
                  g_nSpyroWorldPosY - sign->pos[1], 0);
  }
  a = yaw & 0xff;
  ang = a * 16;
  radial[0] = (LookupCosine(ang) * 3) >> 4;
  radial[1] = (LookupSine(ang) * 3) >> 4;
  radial[2] = 0;
  a = a + 0x40;
  a = a * 16;
  spacing[0] = LookupCosine(a) >> 5;
  spacing[1] = LookupSine(a) >> 5;
  spacing[2] = 0;

  if (named != 0) {
    if (sign->slot == 0) {
      yaw = sign->yaw - 0x80;
    } else {
      yaw = sign->yaw;
      ZeroVector(cursor);
      SubtractVector(radial, cursor, radial);
      SubtractVector(spacing, cursor, spacing);
    }
  }

  len = strlen(name);
  n = len - 1;
  ScaleVector3Sat(cursor, spacing, n);
  LShiftVector3(spacing, 1);

  step = n * 2;
  b = step & 0xff;
  b = b * 16;
  cursor[2] = cursor[2] + ((LookupCosine(b) * 3) >> 3);
  bend[0] = radial[0] * LookupCosine(b);
  bend[1] = radial[1] * LookupCosine(b);
  bend[2] = 0;

  i = 0;
  if (len > 0) {
    p = name;
    do {
      int raw = *p;
      int ch = raw & 0xff;
      int t;
      int skin;

      if (ch != ' ') {
        if ((unsigned int)(raw - 'A') < 26) {
          ch = ch + 0x169;
        } else {
          ch = 0x4c;
        }
        letter = (*(struct Actor * (*)(int, struct Actor *))
                      g_pfnLevelOverlayInitHook)(ch, sign);
        REC(letter)->owner = sign;
        REC(letter)->count = len;
        REC(letter)->index = i;

        t = step & 0xff;
        t = t * 16;
        letter->pos[0] = radial[0] * LookupCosine(t);
        letter->pos[1] = radial[1] * LookupCosine(t);
        letter->pos[2] = 0;

        pos = letter->pos;
        SubtractVector(pos, pos, bend);
        RShiftVector3(pos, 0xa);
        AddVector(pos, pos, radial);
        AddVector(pos, pos, sign->pos);
        SubtractVector(pos, pos, cursor);

        pos[2] = pos[2] + ((LookupCosine(t) * 3) >> 3);
        if (letter->kind == 0x4c) {
          pos[2] = pos[2] + 0x100;
        }
        letter->yaw = yaw;
        letter->yaw16 = yaw & 0xff;
        skin = sign->slot;
        letter->slot = i * 8;
        letter->mode = 2;
        letter->skin = skin;
      }
      SubtractVector(cursor, cursor, spacing);
      step -= 4;
      i++;
      p++;
    } while (i < len);
  }
}
