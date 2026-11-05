/* rot_billboard.h — the g_aRotBillboards record: 8 rotating sprite billboards
 * (sparkles around items) that SpawnActorRotBillboard fills and
 * DrawWorldRotatingBillboards ages and draws. */

#ifndef OPEN_SPYRO_ROT_BILLBOARD_H
#define OPEN_SPYRO_ROT_BILLBOARD_H

typedef struct RotBillboard {
  int pos[3];             /* 0x00 world position */
  unsigned char life;     /* 0x0C frames left; 0 = slot free */
  unsigned char lifePeak; /* 0x0D size-envelope peak & fade divisor */
  unsigned char angle;    /* 0x0E spin phase into the sine LUT */
  signed char angVel;     /* 0x0F signed spin speed */
  unsigned char r, g, b;  /* 0x10 color (word is OR'd with poly code) */
  unsigned char pad13;
  unsigned char halfSize;  /* 0x14 base sprite half-size */
  unsigned char cullDepth; /* 0x15 max screen z, in units of 0x100 */
  unsigned char pad16[2];
} RotBillboard; /* 0x18 */

/* An actor keeps the glint SpawnActorRotBillboard just gave it (follows it
 * each frame) only while Spyro is within this 2D distance; past it the
 * billboard is left to fade where it spawned. */
#define ROT_BILLBOARD_ATTACH_RANGE 0x4000

#endif /* OPEN_SPYRO_ROT_BILLBOARD_H */
