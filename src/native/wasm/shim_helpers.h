/* src/native/wasm/shim_helpers.h — the SDK shim's (sdk_shim.c) packet, GTE
 * and libcd bookkeeping, kept free of PsyZ's headers so native/tests can test
 * it (shim_helpers_test.c). */

#ifndef OPEN_SPYRO_SHIM_HELPERS_H
#define OPEN_SPYRO_SHIM_HELPERS_H

/* Clears the bits above the E1 bits 0-8 in the texpage halfword of every
   textured polygon in a GPU packet of `len` words. Stops at the first command
   whose length it does not know. */
void shim_mask_poly_tpages(unsigned long *cmd, unsigned int len);

/* out = m0 * m1 (4.12 fixed point), as the GTE's MulMatrix0 computes it: each
   product sum shifted right by 12 and saturated to 16 bits. out may be m0 or
   m1. */
void shim_mul_matrix0(short m0[3][3], short m1[3][3], short out[3][3]);

/* libcd's CdlGetlocP (libcd.h; sdk_shim.c checks they agree). */
#define SHIM_CDL_GETLOCP 0x11

/* The last libcd command the game issued, and the CdlGetlocP result built
   for the next CdSync. */
typedef struct {
  unsigned char last_com;
  unsigned char getlocp[8];
} ShimCdTrack;

/* Records `com`. `locl` is the CdlGetlocL result (its first three bytes, the
   absolute MSF) when com is CdlGetlocP and it could be read, else NULL; a
   GetlocP without one keeps the previous position. */
void shim_cd_track(ShimCdTrack *t, unsigned char com,
                   const unsigned char *locl);

/* After CdSync: while the last command is CdlGetlocP, its result replaces
   the 8 bytes at `result` (when not NULL). */
void shim_cd_sync_result(const ShimCdTrack *t, unsigned char *result);

#endif
