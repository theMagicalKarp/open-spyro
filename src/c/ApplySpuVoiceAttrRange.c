#include "globals.h"

/* libspu SpuVoiceAttr. */
typedef struct {
  unsigned int voice;         /* 0x00 */
  unsigned int mask;          /* 0x04 */
  short volume_left;          /* 0x08 */
  short volume_right;         /* 0x0a */
  short volmode_left;         /* 0x0c */
  short volmode_right;        /* 0x0e */
  short volumex_left;         /* 0x10 */
  short volumex_right;        /* 0x12 */
  unsigned short pitch;       /* 0x14 */
  unsigned short note;        /* 0x16 */
  unsigned short sample_note; /* 0x18 */
  short envx;                 /* 0x1a */
  unsigned int addr;          /* 0x1c */
  unsigned int loop_addr;     /* 0x20 */
  int a_mode;                 /* 0x24 */
  int s_mode;                 /* 0x28 */
  int r_mode;                 /* 0x2c */
  unsigned short ar;          /* 0x30 */
  unsigned short dr;          /* 0x32 */
  unsigned short sr;          /* 0x34 */
  unsigned short rr;          /* 0x36 */
  unsigned short sl;          /* 0x38 */
  unsigned short adsr1;       /* 0x3a */
  unsigned short adsr2;       /* 0x3c */
} SpuVoiceAttr;

/* One SPU voice's register block (0x10 bytes). */
typedef struct {
  unsigned short volume_left;  /* 0x0 */
  unsigned short volume_right; /* 0x2 */
  unsigned short pitch;        /* 0x4 */
  unsigned short addr;         /* 0x6 */
  unsigned short adsr1;        /* 0x8 */
  unsigned short adsr2;        /* 0xa */
  unsigned short volumex;      /* 0xc */
  unsigned short loop_addr;    /* 0xe */
} SpuVoiceRegs;

extern volatile unsigned short *D_80073554; /* SPU register block base */

extern uint ComputeSpuPitchFromNoteDelta(uint, uint, uint, uint);
extern uint WriteSpuVoiceAddrRegister(int idx, uint addr);
extern void DelaySpuRegisterWrite(void);

/* libspu _SpuSetVoiceAttr (0x8005cfec, 1596 bytes): write every field of
   `attr` selected by attr->mask (mask 0 = all) into the register block of
   each voice in [lo..hi] whose bit is set in attr->voice (lo == hi forces
   that voice). Volume / ADSR mode codes select the sweep and slope bits; the
   rates are clamped to their register widths. Unless `no_flush`, finishes
   with the register-write fence. Returns -3 on an empty/invalid range. */
int ApplySpuVoiceAttrRange(SpuVoiceAttr *attr, int lo, int hi, int no_flush) {
  int ch;
  int ch8;
  unsigned int mask;
  int all;
  int single;
  unsigned short vl;
  unsigned short vr;
  unsigned short vmode;
  unsigned short vol;
  int val;
  int rate;
  int rmode;

  vl = 0;
  vr = 0;
  single = lo == hi;
  mask = attr->mask;
  all = mask == 0;

  if (lo < 0) {
    lo = 0;
  }
  if (lo >= 24) {
    return -3;
  }
  if (hi >= 24) {
    hi = 23;
  }
  if (hi < 0 || hi < lo) {
    return -3;
  }
  hi++;

  for (ch = lo; ch < hi; ch++) {
    if (!single && !(attr->voice & (1 << ch))) {
      continue;
    }
    ch8 = ch << 3;

    if (all || (mask & 0x10)) {
      ((SpuVoiceRegs *)D_80073554)[ch].pitch = attr->pitch;
    }
    if (all || (mask & 0x40)) {
      g_anSpuVoiceNoteShadow[ch] = attr->sample_note;
    }
    if (all || (mask & 0x20)) {
      D_80073554[ch8 + 2] = ComputeSpuPitchFromNoteDelta(
          g_anSpuVoiceNoteShadow[ch] >> 8, g_anSpuVoiceNoteShadow[ch] & 0xFF,
          attr->note >> 8, attr->note & 0xFF);
    }
    if (all || (mask & 0x20000)) {
      D_80073554[ch8 + 4] = attr->adsr1;
    }
    if (all || (mask & 0x40000)) {
      D_80073554[ch8 + 5] = attr->adsr2;
    }

    if (all || (mask & 1)) {
      if (all || (mask & 4)) {
        switch (attr->volmode_left) {
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
          vl = attr->volume_left;
          vmode = 0;
          break;
        }
      } else {
        vl = attr->volume_left;
        vmode = 0;
      }
      if (vmode != 0) {
        if (attr->volume_left >= 0x80) {
          vl = 0x7F;
        } else if (attr->volume_left < 0) {
          vl = 0;
        } else {
          vl = attr->volume_left;
        }
      }
      vol = vl & 0x7FFF;
      D_80073554[ch8 + 0] = vol | vmode;
    }

    if (all || (mask & 2)) {
      if (all || (mask & 8)) {
        switch (attr->volmode_right) {
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
          vr = attr->volume_right;
          vmode = 0;
          break;
        }
      } else {
        vr = attr->volume_right;
        vmode = 0;
      }
      if (vmode != 0) {
        if (attr->volume_right >= 0x80) {
          vr = 0x7F;
        } else if (attr->volume_right < 0) {
          vr = 0;
        } else {
          vr = attr->volume_right;
        }
      }
      vol = vr & 0x7FFF;
      D_80073554[ch8 + 1] = vol | vmode;
    }

    if (all || (mask & 0x10000)) {
      WriteSpuVoiceAddrRegister(ch8 | 7, attr->loop_addr);
    }
    if (all || (mask & 0x80)) {
      WriteSpuVoiceAddrRegister(ch8 | 3, attr->addr);
    }

    if (all || (mask & 0x800)) {
      if (all || (mask & 0x100)) {
        switch (attr->a_mode) {
        case 1:
          rmode = 0;
          break;
        case 5:
          rmode = 0x80;
          break;
        default:
          rmode = 0;
          break;
        }
      } else {
        rmode = 0;
      }
      if (attr->ar >= 0x80) {
        rate = 0x7F;
      } else {
        rate = attr->ar;
      }
      val = D_80073554[ch8 + 4] & 0xFF;
      D_80073554[ch8 + 4] = val | ((rate | rmode) << 8);
    }
    if (all || (mask & 0x1000)) {
      if (attr->dr >= 0x10) {
        rate = 0xF;
      } else {
        rate = attr->dr;
      }
      val = D_80073554[ch8 + 4] & 0xFF0F;
      D_80073554[ch8 + 4] = val | (rate << 4);
    }
    if (all || (mask & 0x8000)) {
      if (attr->sl >= 0x10) {
        rate = 0xF;
      } else {
        rate = attr->sl;
      }
      D_80073554[ch8 + 4] = rate | (D_80073554[ch8 + 4] & 0xFFF0);
    }

    if (all || (mask & 0x2000)) {
      if (all || (mask & 0x200)) {
        switch (attr->s_mode) {
        case 1:
          rmode = 0;
          break;
        case 3:
          rmode = 0x100;
          break;
        case 5:
          rmode = 0x200;
          break;
        case 7:
          rmode = 0x300;
          break;
        default:
          rmode = 0x100;
          break;
        }
      } else {
        rmode = 0;
      }
      if (attr->sr >= 0x80) {
        rate = 0x7F;
      } else {
        rate = attr->sr;
      }
      val = D_80073554[ch8 + 5] & 0x3F;
      D_80073554[ch8 + 5] = val | ((rate | rmode) << 6);
    }
    if (all || (mask & 0x4000)) {
      if (all || (mask & 0x400)) {
        switch (attr->r_mode) {
        case 3:
          rmode = 0;
          break;
        case 7:
          rmode = 0x20;
          break;
        default:
          rmode = 0;
          break;
        }
      } else {
        rmode = 0;
      }
      if (attr->rr >= 0x20) {
        rate = 0x1F;
      } else {
        rate = attr->rr;
      }
      val = D_80073554[ch8 + 5] & 0xFFC0;
      D_80073554[ch8 + 5] = val | (rate | rmode);
    }
  }

  if (no_flush == 0) {
    DelaySpuRegisterWrite();
  }
  return 0;
}
