#include "globals.h"

/* SPU hardware register block (only the fields this file touches). */
typedef struct {
  unsigned short voice[0xC0];  /* 0x000 */
  short mvol_left;             /* 0x180 */
  short mvol_right;            /* 0x182 */
  unsigned short pad184[0x13]; /* 0x184 */
  unsigned short spucnt;       /* 0x1aa */
  unsigned short pad1ac[2];    /* 0x1ac */
  short cd_vol_left;           /* 0x1b0 */
  short cd_vol_right;          /* 0x1b2 */
  short ext_vol_left;          /* 0x1b4 */
  short ext_vol_right;         /* 0x1b6 */
} SpuRegs;

/* libspu SpuCommonAttr. */
typedef struct {
  unsigned int mask;    /* 0x00 */
  short mvol_left;      /* 0x04 */
  short mvol_right;     /* 0x06 */
  short mvolmode_left;  /* 0x08 */
  short mvolmode_right; /* 0x0a */
  short mvolx_left;     /* 0x0c */
  short mvolx_right;    /* 0x0e */
  short cd_vol_left;    /* 0x10 */
  short cd_vol_right;   /* 0x12 */
  int cd_reverb;        /* 0x14 */
  int cd_mix;           /* 0x18 */
  short ext_vol_left;   /* 0x1c */
  short ext_vol_right;  /* 0x1e */
  int ext_reverb;       /* 0x20 */
  int ext_mix;          /* 0x24 */
} SpuCommonAttr;

extern SpuRegs *D_80073554; /* SPU register block base */

/* libspu SpuSetCommonAttr (0x8005cc58, 916 bytes): writes the master volume
   (with the sweep mode from mvolmode, the sweep rate clamped to 0..0x7f), the
   CD/external input volumes and the CD/external reverb + mix enable bits of
   SPUCNT for every field selected in attr->mask (mask 0 = all).

   The register pins reproduce the original's allocation: mask/all in t1/t2
   leave t0 to vr, each clamp's sign-extended read sits in a3/a2, the SPUCNT
   read-modify-writes keep the value in v1, and the last mask test's result
   is pinned to v0 (a dying hard reg would otherwise lend it t1). */
void SetSpuCommonAttr(SpuCommonAttr *attr) {
  register unsigned int mask asm("$9");
  register int all asm("$10");
  unsigned short vl;
  unsigned short vr;
  unsigned short vmode;
  unsigned short vol;

  vl = 0;
  vr = 0;
  mask = attr->mask;
  all = mask == 0;

  if (all || (mask & 1)) {
    if (all || (mask & 4)) {
      switch (attr->mvolmode_left) {
      case 1:
        vmode = 0x8000;
        break;
      case 2:
        vmode = 0x9000;
        break;
      case 3:
        vmode = 0xA000;
        break;
      case 4:
        vmode = 0xB000;
        break;
      case 5:
        vmode = 0xC000;
        break;
      case 6:
        vmode = 0xD000;
        break;
      case 7:
        vmode = 0xE000;
        break;
      case 0:
      default:
        vl = attr->mvol_left;
        vmode = 0;
        break;
      }
    } else {
      vl = attr->mvol_left;
      vmode = 0;
    }
    if (vmode != 0) {
      register int t asm("$7") = attr->mvol_left;

      if (t >= 0x80) {
        vl = 0x7F;
      } else if (t < 0) {
        vl = 0;
      } else {
        vl = attr->mvol_left;
      }
    }
    vol = vl & 0x7FFF;
    D_80073554->mvol_left = vol | vmode;
  }

  if (all || (mask & 2)) {
    if (all || (mask & 8)) {
      switch (attr->mvolmode_right) {
      case 1:
        vmode = 0x8000;
        break;
      case 2:
        vmode = 0x9000;
        break;
      case 3:
        vmode = 0xA000;
        break;
      case 4:
        vmode = 0xB000;
        break;
      case 5:
        vmode = 0xC000;
        break;
      case 6:
        vmode = 0xD000;
        break;
      case 7:
        vmode = 0xE000;
        break;
      case 0:
      default:
        vr = attr->mvol_right;
        vmode = 0;
        break;
      }
    } else {
      vr = attr->mvol_right;
      vmode = 0;
    }
    if (vmode != 0) {
      register int t asm("$6") = attr->mvol_right;

      if (t >= 0x80) {
        vr = 0x7F;
      } else if (t < 0) {
        vr = 0;
      } else {
        vr = attr->mvol_right;
      }
    }
    vol = vr & 0x7FFF;
    D_80073554->mvol_right = vol | vmode;
  }

  if (all || (mask & 0x40)) {
    D_80073554->cd_vol_left = attr->cd_vol_left;
  }
  if (all || (mask & 0x80)) {
    D_80073554->cd_vol_right = attr->cd_vol_right;
  }
  if (all || (mask & 0x400)) {
    D_80073554->ext_vol_left = attr->ext_vol_left;
  }
  if (all || (mask & 0x800)) {
    D_80073554->ext_vol_right = attr->ext_vol_right;
  }
  if (all || (mask & 0x100)) {
    if (attr->cd_reverb == 0) {
      {
        register unsigned short s asm("$3") = D_80073554->spucnt;

        D_80073554->spucnt = s & ~4;
      }
    } else {
      {
        register unsigned short s asm("$3") = D_80073554->spucnt;

        D_80073554->spucnt = s | 4;
      }
    }
  }
  if (all || (mask & 0x200)) {
    if (attr->cd_mix == 0) {
      {
        register unsigned short s asm("$3") = D_80073554->spucnt;

        D_80073554->spucnt = s & ~1;
      }
    } else {
      {
        register unsigned short s asm("$3") = D_80073554->spucnt;

        D_80073554->spucnt = s | 1;
      }
    }
  }
  if (all || (mask & 0x1000)) {
    if (attr->ext_reverb == 0) {
      {
        register unsigned short s asm("$3") = D_80073554->spucnt;

        D_80073554->spucnt = s & ~8;
      }
    } else {
      {
        register unsigned short s asm("$3") = D_80073554->spucnt;

        D_80073554->spucnt = s | 8;
      }
    }
  }
  {
    register unsigned int c asm("$2") = mask & 0x2000;

    if (all || c) {
      if (attr->ext_mix == 0) {
        {
          register unsigned short s asm("$3") = D_80073554->spucnt;

          D_80073554->spucnt = s & ~2;
        }
      } else {
        {
          register unsigned short s asm("$3") = D_80073554->spucnt;

          D_80073554->spucnt = s | 2;
        }
      }
    }
  }
}
