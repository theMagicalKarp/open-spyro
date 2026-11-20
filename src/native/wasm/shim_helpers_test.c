/* src/native/wasm/shim_helpers_test.c — unit tests for shim_helpers.c (make
 * native-test). */

#include <string.h>

#include "shim_helpers.h"
#include "test.h"

/* --- texpage masking ----------------------------------------------------- */

/* A texpage word: flag bits 13-15 set above texpage 0x1F, UV 0x3344. */
#define TPAGE_WORD 0xE01F3344u
#define TPAGE_MASKED 0x001F3344u
#define UV_WORD 0xABCD5566u /* a clut / plain UV word: never touched */

TEST(shim_mask_flat_textured_tri) {
  /* POLY_FT3 (0x24): color, xy0, uv0+clut, xy1, uv1+tpage, xy2, uv2 */
  unsigned long p[] = {0x24808080, 0, UV_WORD, 0, TPAGE_WORD, 0, UV_WORD};

  shim_mask_poly_tpages(p, 7);
  CHECK_INT(p[4], TPAGE_MASKED);
  CHECK_INT(p[2], UV_WORD);
  CHECK_INT(p[6], UV_WORD);
}

TEST(shim_mask_gouraud_textured_quad) {
  /* POLY_GT4 (0x3C): 12 words, tpage at word 5 */
  unsigned long p[12] = {0x3C808080, 0, UV_WORD, 0, 0, TPAGE_WORD};

  shim_mask_poly_tpages(p, 12);
  CHECK_INT(p[5], TPAGE_MASKED);
  CHECK_INT(p[2], UV_WORD);
}

TEST(shim_mask_untextured_untouched) {
  /* POLY_G3 (0x30): 6 words, no UVs */
  unsigned long p[] = {0x30808080, 0, 0x00112233, 0, TPAGE_WORD, 0};

  shim_mask_poly_tpages(p, 6);
  CHECK_INT(p[4], TPAGE_WORD);
}

TEST(shim_mask_walks_packet) {
  /* a draw-mode word, a nop, POLY_FT4 (9 words), then POLY_FT3 */
  unsigned long p[] = {
      0xE1000200, 0x00000000, /* */
      0x2C808080, 0,          UV_WORD, 0, TPAGE_WORD, 0, UV_WORD,
      0,          UV_WORD,                                        /* */
      0x24808080, 0,          UV_WORD, 0, TPAGE_WORD, 0, UV_WORD, /* */
  };

  shim_mask_poly_tpages(p, sizeof(p) / sizeof(p[0]));
  CHECK_INT(p[0], 0xE1000200); /* commands other than polygons unchanged */
  CHECK_INT(p[6], TPAGE_MASKED);
  CHECK_INT(p[15], TPAGE_MASKED);
}

TEST(shim_mask_stops_at_unknown_command) {
  /* a line (0x40): its length is not decoded, so nothing after is masked */
  unsigned long p[] = {0x40808080, 0, 0,          0x24808080, 0,
                       UV_WORD,    0, TPAGE_WORD, 0,          UV_WORD};

  shim_mask_poly_tpages(p, 10);
  CHECK_INT(p[7], TPAGE_WORD);
}

TEST(shim_mask_truncated_polygon) {
  /* a POLY_FT3 cut off before its last word: left alone, no overrun */
  unsigned long p[] = {0x24808080, 0, UV_WORD, 0, TPAGE_WORD, 0};

  shim_mask_poly_tpages(p, 6);
  CHECK_INT(p[4], TPAGE_WORD);
}

/* --- MulMatrix0 ---------------------------------------------------------- */

#define ONE 0x1000

TEST(shim_mul_matrix_identity) {
  short id[3][3] = {{ONE, 0, 0}, {0, ONE, 0}, {0, 0, ONE}};
  short m[3][3] = {{1, 2, 3}, {-4, 5, -6}, {0x7FFF, -0x8000, 9}};
  short out[3][3];

  shim_mul_matrix0(id, m, out);
  CHECK(memcmp(out, m, sizeof(m)) == 0);
  shim_mul_matrix0(m, id, out);
  CHECK(memcmp(out, m, sizeof(m)) == 0);
}

TEST(shim_mul_matrix_rotations) {
  /* two 90-degree turns about z make a 180-degree turn */
  short rz[3][3] = {{0, -ONE, 0}, {ONE, 0, 0}, {0, 0, ONE}};
  short out[3][3];

  shim_mul_matrix0(rz, rz, out);
  CHECK_INT(out[0][0], -ONE);
  CHECK_INT(out[0][1], 0);
  CHECK_INT(out[1][1], -ONE);
  CHECK_INT(out[2][2], ONE);
}

TEST(shim_mul_matrix_fixed_point_and_saturation) {
  short half[3][3] = {{ONE / 2, 0, 0}, {0, ONE / 2, 0}, {0, 0, ONE / 2}};
  short big[3][3] = {{0x7FFF, 0, 0}, {0, 0, 0}, {0, 0, 0}};
  short out[3][3];

  shim_mul_matrix0(half, half, out);
  CHECK_INT(out[0][0], ONE / 4);
  /* 0x7FFF * 0x7FFF >> 12 = 0x3FFF0: saturates */
  short col[3][3] = {{0x7FFF, 0, 0}, {0, 0, 0}, {0, 0, 0}};
  shim_mul_matrix0(big, col, out);
  CHECK_INT(out[0][0], 0x7FFF);
  big[0][0] = -0x8000;
  shim_mul_matrix0(big, col, out);
  CHECK_INT(out[0][0], -0x8000);
  /* >> 12 rounds toward minus infinity */
  short a[3][3] = {{-1, 0, 0}, {0, 0, 0}, {0, 0, 0}};
  short b[3][3] = {{1, 0, 0}, {0, 0, 0}, {0, 0, 0}};
  shim_mul_matrix0(a, b, out);
  CHECK_INT(out[0][0], -1);
}

TEST(shim_mul_matrix_in_place) {
  short rz[3][3] = {{0, -ONE, 0}, {ONE, 0, 0}, {0, 0, ONE}};

  shim_mul_matrix0(rz, rz, rz);
  CHECK_INT(rz[0][0], -ONE);
  CHECK_INT(rz[1][0], 0);
}

/* --- CdlGetlocP ---------------------------------------------------------- */

enum { CDL_GETLOCL = 0x10, CDL_READN = 0x06 };

TEST(shim_cd_getlocp_from_getlocl) {
  ShimCdTrack t = {0};
  const unsigned char locl[8] = {0x12, 0x34, 0x56, 0x99, 0x99};
  unsigned char result[8];

  shim_cd_track(&t, SHIM_CDL_GETLOCP, locl);
  CHECK_INT(t.last_com, SHIM_CDL_GETLOCP);
  memset(result, 0xEE, sizeof(result));
  shim_cd_sync_result(&t, result);
  CHECK(memcmp(result, "\x01\x01\x12\x34\x56\x12\x34\x56", 8) == 0);
}

TEST(shim_cd_result_only_after_getlocp) {
  ShimCdTrack t = {0};
  const unsigned char locl[3] = {1, 2, 3};
  unsigned char result[8];

  shim_cd_track(&t, SHIM_CDL_GETLOCP, locl);
  shim_cd_track(&t, CDL_READN, NULL);
  CHECK_INT(t.last_com, CDL_READN);
  memset(result, 0xEE, sizeof(result));
  shim_cd_sync_result(&t, result);
  CHECK_INT(result[0], 0xEE);
  CHECK_INT(result[7], 0xEE);
  shim_cd_sync_result(&t, NULL); /* no result buffer: nothing to do */
}

TEST(shim_cd_getlocp_without_position_keeps_last) {
  ShimCdTrack t = {0};
  const unsigned char locl[3] = {7, 8, 9};
  unsigned char result[8];

  shim_cd_track(&t, SHIM_CDL_GETLOCP, locl);
  shim_cd_track(&t, CDL_GETLOCL, NULL);
  shim_cd_track(&t, SHIM_CDL_GETLOCP, NULL); /* GetlocL failed */
  shim_cd_sync_result(&t, result);
  CHECK_INT(result[5], 7);
  CHECK_INT(result[7], 9);
  /* locl only counts for GetlocP */
  shim_cd_track(&t, CDL_READN, (const unsigned char *)"\x05\x05\x05");
  CHECK_INT(t.getlocp[5], 7);
}
