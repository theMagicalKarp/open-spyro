#include "actor_type.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* The per-letter banner record hung off a letter actor's ->state pointer (a
   sign actor's ->state holds its level id instead). */
struct NameRec {
  void *owner; /* 0x00 */
  short count; /* 0x04 */
  short index; /* 0x06 */
};

/* The banner record pointer (->state) is re-read before every field store
   (the original never proves the letter actor's +0 stays put across them). */
#define REC(p) (*(struct NameRec *volatile *)&(p)->state)

/* Spells a name out in floating letter actors around `sign`. With `named` set
   the text is the level name for the sign's own level id and the arc hangs off
   the sign's yaw (mirrored, and flipped through the origin, when the sign's
   ->subMode is non-zero); otherwise it is the balloonist's name laid out
   facing Spyro. Letters are spawned through g_pfnLevelSpawnActor ('A'-'Z'
   map to id 0x1AA.., anything else to 0x4C) and threaded onto a radial arc:
   each glyph sits one `spacing` step further round the ring, bent outward by
   `bend` and lifted by the cosine of its position ('L' actors get an extra
   0x100 of height). Spaces advance the cursor without spawning.
   (0x8003c358, 908 bytes.) */
void SpawnNameLetterActors(Actor *sign, int named) {
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
  Actor *letter;
  int *pos;

  if (named != 0) {
    name = g_apLevelNameStrings[*(int *)sign->state];
    yaw = sign->rotZ;
  } else {
    name = g_apBalloonistNames[0];
    yaw = ArcTan2(g_anSpyroWorldPos[0] - sign->posX,
                  g_nSpyroWorldPosY - sign->posY, 0);
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
    if (sign->subMode == 0) {
      yaw = sign->rotZ - 0x80;
    } else {
      yaw = sign->rotZ;
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
          ch = ch + (ACTOR_TYPE_LETTER_A - 'A');
        } else {
          ch = ACTOR_TYPE_GLYPH_COMMA;
        }
        letter = (*(Actor * (*)(int, Actor *)) g_pfnLevelSpawnActor)(ch, sign);
        REC(letter)->owner = sign;
        REC(letter)->count = len;
        REC(letter)->index = i;

        t = step & 0xff;
        t = t * 16;
        letter->posX = radial[0] * LookupCosine(t);
        letter->posY = radial[1] * LookupCosine(t);
        letter->posZ = 0;

        pos = &letter->posX;
        SubtractVector(pos, pos, bend);
        RShiftVector3(pos, 0xa);
        AddVector(pos, pos, radial);
        AddVector(pos, pos, &sign->posX);
        SubtractVector(pos, pos, cursor);

        pos[2] = pos[2] + ((LookupCosine(t) * 3) >> 3);
        if (letter->type == ACTOR_TYPE_GLYPH_COMMA) {
          pos[2] = pos[2] + 0x100;
        }
        letter->rotZ = yaw;
        letter->floorOffset = yaw & 0xff;
        skin = sign->subMode;
        letter->subMode = i * 8;
        letter->tintMode = 2;
        letter->mode = skin;
      }
      SubtractVector(cursor, cursor, spacing);
      step -= 4;
      i++;
      p++;
    } while (i < len);
  }
}
