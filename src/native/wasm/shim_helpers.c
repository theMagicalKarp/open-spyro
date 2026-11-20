/* src/native/wasm/shim_helpers.c — see shim_helpers.h. */

#include "shim_helpers.h"

#include <string.h>

/* A textured polygon's texpage halfword (the top of its second UV word)
 * carries the E1 bits 0-8; the GPU ignores the bits above. PsyZ's backends
 * use those bits as private flags (bit 15 = untextured, 13 = polygon,
 * 14 = dither), and the world renderer's texture records set bit 15, so
 * every world polygon past the near detail pass drew untextured. Clear them
 * in each polygon of a packet. */
void shim_mask_poly_tpages(unsigned long *cmd, unsigned int len) {
  unsigned int i = 0;

  while (i < len) {
    unsigned int code = (unsigned int)(cmd[i] >> 24) & 0xFF;
    unsigned int n;

    if ((code & 0xE0) == 0x20) { /* polygon */
      unsigned int verts = code & 0x08 ? 4 : 3;
      unsigned int tex = (code >> 2) & 1, gouraud = (code >> 4) & 1;

      n = 1 + verts * (1 + tex) + gouraud * (verts - 1);
      if (tex && i + n <= len) {
        cmd[i + 4 + gouraud] &= 0x01FFFFFFu;
      }
    } else if (code == 0x00 || (code >= 0xE1 && code <= 0xE6)) {
      n = 1;
    } else {
      return;
    }
    i += n;
  }
}

static short sat16(int v) {
  return v < -0x8000 ? -0x8000 : v > 0x7FFF ? 0x7FFF : (short)v;
}

void shim_mul_matrix0(short m0[3][3], short m1[3][3], short out[3][3]) {
  short r[3][3];
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      int s = m0[i][0] * m1[0][j] + m0[i][1] * m1[1][j] + m0[i][2] * m1[2][j];
      r[i][j] = sat16(s >> 12);
    }
  }
  memcpy(out, r, sizeof(r));
}

/* PsyZ does not implement CdlGetlocP. Its result is built from CdlGetlocL
 * (the XA read position while streaming) and handed back by the next
 * CdSync, as the PS1's interrupt would: the game reads only the absolute MSF
 * (bytes 5..7) to keep its music read head, which is where a resume after
 * the pause menu restarts the stream. */
void shim_cd_track(ShimCdTrack *t, unsigned char com,
                   const unsigned char *locl) {
  if (com == SHIM_CDL_GETLOCP && locl != NULL) {
    t->getlocp[0] = 1; /* track */
    t->getlocp[1] = 1; /* index */
    t->getlocp[2] = locl[0];
    t->getlocp[3] = locl[1];
    t->getlocp[4] = locl[2];
    t->getlocp[5] = locl[0];
    t->getlocp[6] = locl[1];
    t->getlocp[7] = locl[2];
  }
  t->last_com = com;
}

void shim_cd_sync_result(const ShimCdTrack *t, unsigned char *result) {
  if (result != NULL && t->last_com == SHIM_CDL_GETLOCP) {
    memcpy(result, t->getlocp, sizeof(t->getlocp));
  }
}
