#include "globals.h"

extern int VSync(int mode);
extern void WriteString(char *s);
extern int WritePrintf();
extern void CdResetController(void);

extern char D_80011DFC[]; /* "CD timeout: " */
extern char D_80011E0C[]; /* "%s:(%s) Sync=%s, Ready=%s\n" */
extern char D_80011F04[]; /* "CD_datasync" */

extern int D_80075AE8;           /* VSync deadline stamp */
extern int D_80075AEC;           /* spin counter */
extern char *D_80075AF0;         /* op-name string for the timeout message */
extern int *D_80075140;          /* libcd status word pointer */
extern int D_80074E5C[];         /* sync-status name table */
extern int D_80074EDC[];         /* ready-status name table */
extern unsigned char D_80074E55; /* sync index */
extern unsigned char D_80075114; /* ready index (Sync) */
extern unsigned char D_80075115; /* ready index (Ready) */

/* libcd CdDataSync (0x800655a0, 0x16c). Spins up to ~0x3C0 VSyncs (or 0x3C0000
   iterations) for the data-ready bit (0x1000000) of *D_80075140. On timeout
   prints "CD timeout: <op>:(<sync>) Sync=.., Ready=.." and resets the drive.
   Returns 1 = still busy (mode != 0 poll), 0 = ready, -1 = timed out.

   Load-bearing shapes, do not "simplify":
     - the timeout check is the same inlined libcd helper as CheckGpuTimeout:
       one `||` over the VSync deadline and the post-incremented spin counter,
       arms assigning r = -1 / 0, then `if (r != 0) return -1;` (this gives
       the original's `j` + second `li -1` in the bnez delay slot).
     - `break` out of the `for (;;)` with `return 0` as the function tail and
       `return 1` inside the loop; anything else leaves a 0-arm block (+4).
     - the three table bases are pointer locals assigned AFTER the first
       VSync (declaration order fixes s1/s0/s3; assignment order is the A164
       straggler order, syncTbl first).
     - the printf block is CdSync's recipe: volatile idx[0]/idx[1] index
       temps and a volatile direct name load into a1, so the format-string
       set lands last and the idx[0] address chain takes a0. The real loop
       already doubles the block's refs, so no do/while(0) is needed here. */
int CdDataSync(int mode) {
  unsigned char *readyIdx;
  int *readyTbl;
  int *syncTbl;
  int r;

  D_80075AE8 = VSync(-1) + 0x3C0;
  syncTbl = D_80074E5C;
  readyIdx = &D_80075114;
  readyTbl = D_80074EDC;
  D_80075AEC = 0;
  D_80075AF0 = D_80011F04;

  for (;;) {
    if (VSync(-1) > D_80075AE8 || D_80075AEC++ > 0x3C0000) {
      WriteString(D_80011DFC);
      {
        int i0 = ((volatile unsigned char *)readyIdx)[0];
        int i1 = ((volatile unsigned char *)readyIdx)[1];
        WritePrintf(D_80011E0C, *(char *volatile *)&D_80075AF0,
                    syncTbl[D_80074E55], readyTbl[i0], readyTbl[i1]);
      }
      CdResetController();
      r = -1;
    } else {
      r = 0;
    }
    if (r != 0) {
      return -1;
    }
    if ((*D_80075140 & 0x1000000) == 0) {
      break;
    }
    if (mode != 0) {
      return 1;
    }
  }
  return 0;
}
