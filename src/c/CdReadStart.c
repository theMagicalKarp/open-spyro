#include "globals.h"

extern int WriteString(char *s);
extern int CdControl(int com, unsigned char *param, unsigned char *result);
extern int CdControlForce(int cmd, void *param);
extern int CdStatus(void);
extern unsigned char *CdLastPos(void);
extern int CdPosToInt(unsigned char *pos);
extern void *CdSyncCallback(void *func);
extern void *CdReadyCallback(void *func);
extern int VSync(int mode);
extern void FUN_80063b38(void);
extern int FUN_80063a9c(void);
extern void FUN_80064050();
extern void D_800659F0();          /* sector-ready callback */
extern unsigned char D_80065CC0[]; /* DMA delivery callback */

extern char D_80011F28[]; /* "CdRead: Shell open...\n" */
extern char D_80011F40[]; /* "CdRead: retry...\n" */

extern int D_80075148;            /* sectors to read */
extern void *D_8007514C;          /* destination buffer */
extern void *volatile D_80075150; /* committed destination buffer */
extern int D_80075154[];          /* libcd read mode */
extern volatile int D_8007515C;   /* sectors still to read, -1 on failure */
extern int D_80075160;            /* VSync stamp at read kick */
extern int D_80075164;            /* VSync stamp on shell-open */
extern int D_80075168;            /* head position at read kick */
extern int D_80075178[];          /* driver flags (bit 0 = DMA delivery) */

/* libcd CdReadStart (0x80065dbc, 532 bytes): (re)issue the latched read.
   Parks the callbacks; if the shell is open, nudges the drive and fails. On a
   retry, pauses and re-seeks to the last position first. Sets the read mode
   if it changed, arms the sector-ready (and DMA) callbacks and issues
   CdlReadN. Returns the pending sector count, -1 on failure. */
int CdReadStart(int retry) {
  unsigned char mode[4];
  int m;

  CdSyncCallback(0);
  CdReadyCallback(0);
  if (*(volatile int *)D_80075178 & 1) {
    FUN_80064050(0);
  }
  if (CdStatus() & 0x10) {
    if ((VSync(-1) & 0x3F) == 0) {
      WriteString(D_80011F28);
    }
    CdControlForce(1, 0);
    D_80075164 = VSync(-1);
    D_8007515C = -1;
    return D_8007515C;
  }
  if (retry != 0) {
    WriteString(D_80011F40);
    CdControl(9, 0, 0);
    if (CdControl(2, CdLastPos(), 0) == 0) {
      D_8007515C = -1;
      return D_8007515C;
    }
  }
  FUN_80063b38();
  m = *(volatile int *)D_80075154;
  mode[0] = m;
  m &= 0xFF;
  if (m != FUN_80063a9c() || retry != 0) {
    if (CdControl(0xE, mode, 0) == 0) {
      D_8007515C = -1;
      return D_8007515C;
    }
  }
  D_80075168 = CdPosToInt(CdLastPos());
  CdReadyCallback(D_800659F0);
  if (D_80075178[0] & 1) {
    FUN_80064050(D_80065CC0);
  }
  D_80075150 = D_8007514C;
  CdControlForce(6, 0);
  D_8007515C = D_80075148;
  D_80075160 = VSync(-1);
  return D_8007515C;
}
