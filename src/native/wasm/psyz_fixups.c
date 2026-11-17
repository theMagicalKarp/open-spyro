/* src/native/wasm/psyz_fixups.c — adapters for calls inside PsyZ whose
 * declaration in the calling file differs from PsyZ's definition.
 *
 * A native build tolerates the difference; wasm traps on any call whose
 * signature differs from the callee's. native/wasm/CMakeLists.txt compiles
 * each calling file with the callee renamed to the PsyzFix_ adapter below,
 * which has the caller's signature and forwards to the real definition. */

long WaitEvent(long event);
long StartCARD2(void);
long StopCARD2(void);
void CD_datasync(int mode);
int CD_getsector2(void);

/* decomp/src/libspu/s_crwa.c: void WaitEvent(s32) */
void PsyzFix_WaitEvent(int event) { WaitEvent(event); }

/* decomp/src/libcard/init.c: void StartCARD2(void), void StopCARD2(void) */
void PsyzFix_StartCARD2(void) { StartCARD2(); }
void PsyzFix_StopCARD2(void) { StopCARD2(); }

/* decomp/src/libcd/sys.c: int CD_datasync(int), int CD_getsector2(void *, int)
 * (both NOT_IMPLEMENTED in PsyZ's libcd.c) */
int PsyzFix_CD_datasync(int mode) {
  CD_datasync(mode);
  return 0;
}
int PsyzFix_CD_getsector2(void *madr, int size) {
  (void)madr;
  (void)size;
  return CD_getsector2();
}
