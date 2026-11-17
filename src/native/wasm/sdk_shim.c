/* src/native/wasm/sdk_shim.c — the SDK functions the game calls, under the
 * names the game C uses for them.
 *
 * The PS1 links the PSY-Q libraries from the original EXE; the matched C
 * calls them by project names (`GetRandomU32` is libc `rand`,
 * `CdControlBlocking` is `CdControlB`, ...). Natively PsyZ provides the
 * libraries, so each project name forwards to its PsyZ function here. What
 * PsyZ lacks (libpad's direct mode, a few kernel calls) is shimmed or
 * stubbed; the libmcrd upper layer is memcard.c. Each definition keeps the
 * wasm signature of the game's prototype in include/funcs.h: a call whose
 * signature differs from the definition traps in wasm. */

#include <psyz.h>
#include <libapi.h>
#include <libcd.h>
#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include <libspu.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHIM_LOG(...) printf("sdk_shim: " __VA_ARGS__)

/* ---- libc ------------------------------------------------------------------ */

unsigned int GetRandomU32(void) { return (unsigned int)rand(); }

int WritePrintf(const char *fmt, ...) {
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = vprintf(fmt, ap);
  va_end(ap);
  return n;
}

/* ---- libapi / kernel -------------------------------------------------------- */

void InstallVSyncCallback(unsigned int cb) { VSyncCallback((void (*)())cb); }

void Bios96Remove(void) { _96_remove(); }

void _96_init(void) { SHIM_LOG("_96_init: ignored\n"); }

/* The libapi vtable slot 0x10 call the boot path makes. No native meaning. */
void CallLibapiSysSlot10(void) { SHIM_LOG("libapi slot 0x10: ignored\n"); }

long Shim_LoadExec(char *file, unsigned long sp, unsigned long sp_size) {
  (void)sp;
  (void)sp_size;
  SHIM_LOG("LoadExec(%s): no other executable natively; exiting\n", file);
  exit(0);
}

/* ---- libgpu ----------------------------------------------------------------- */

DRAWENV *InitDefaultDrawEnv(DRAWENV *env, int x, int y, int w, int h) {
  return SetDefDrawEnv(env, x, y, w, h);
}

DISPENV *InitDefaultDispEnv(DISPENV *env, int x, int y, int w, int h) {
  return SetDefDispEnv(env, x, y, w, h);
}

/* The game builds its ordering tables in the PS1 packet format: one tag
 * word per packet, the next packet's address in the low 24 bits and the
 * packet's word count in the top 8, 0xFFFFFF ending the chain. PsyZ's native
 * libgpu walks a different format (a two-word tag: full next pointer, then
 * length; P_TAG under __psyz). Shim_DrawOTag walks the game's chain and
 * re-emits it in PsyZ's format, flushing to PsyZ's DrawOTag whenever the
 * buffer fills. PsyZ dispatches a DrawOTag before it returns, so the buffer
 * is free again afterwards. */
#define OT_END 0xFFFFFFu
#define OT_BUF_WORDS (64 * 1024)
#define OT_MAX_PACKETS (1 << 20) /* a corrupt chain stops here, not in a loop */

static u_long ot_buf[OT_BUF_WORDS];

/* A textured polygon's texpage halfword (the top of its second UV word)
 * carries the E1 bits 0-8; the GPU ignores the bits above. PsyZ's backends
 * use those bits as private flags (bit 15 = untextured, 13 = polygon,
 * 14 = dither), and the world renderer's texture records set bit 15, so
 * every world polygon past the near detail pass drew untextured. Clear them
 * in each polygon of a packet. The walk stops at the first command whose
 * length it does not know. */
static void mask_poly_tpages(u_long *cmd, unsigned int len) {
  unsigned int i = 0;

  while (i < len) {
    unsigned int code = cmd[i] >> 24;
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

static void ot_flush(int last) {
  u_long *end = &ot_buf[last];

  end[0] = OT_END; /* the last packet ends PsyZ's chain */
  DrawOTag((OT_TYPE *)ot_buf);
}

void Shim_DrawOTag(unsigned int *ot) {
  int pos = 0, last = -1, n = 0;
  unsigned int *p = ot;

  for (;;) {
    unsigned int tag = *p;
    unsigned int len = tag >> 24;
    unsigned int next = tag & 0xFFFFFFu;
    unsigned int i;

    if (pos + 2 + (int)len > OT_BUF_WORDS) {
      if (last >= 0) {
        ot_flush(last);
      }
      pos = 0;
      last = -1;
    }
    if (last >= 0) {
      ot_buf[last] = (u_long)&ot_buf[pos];
    }
    last = pos;
    ot_buf[pos] = OT_END;
    ot_buf[pos + 1] = len;
    for (i = 0; i < len; i++) {
      ot_buf[pos + 2 + i] = p[1 + i];
    }
    mask_poly_tpages(&ot_buf[pos + 2], len);
    pos += 2 + (int)len;
    if (next == OT_END) {
      break;
    }
    if (++n >= OT_MAX_PACKETS) {
      SHIM_LOG("DrawOTag: chain from %p does not end\n", (void *)ot);
      break;
    }
    /* A 24-bit link is a KSEG0 RAM address with its top byte dropped. */
    p = (unsigned int *)(uintptr_t)(0x80000000u | next);
  }
  ot_flush(last);
}

/* PsyZ's SetDrawMode writes PsyZ's native packet (a two-word tag), which
 * would put the E1/E2 words one slot late in the game's 3-word PS1 DR_MODE
 * and overwrite the tag of the primitive the game packs right after it. This
 * builds the PS1 packet: tag length 2, then the draw-mode and texture-window
 * commands, from PsyZ's own encoders. */
u_long get_mode(int dfe, int dtd, u_short tpage); /* PsyZ libgpu/sys.c */
u_long get_tw(RECT *rect);

void Shim_SetDrawMode(void *p, int dfe, int dtd, int tpage, RECT *tw) {
  unsigned int *w = p;

  w[0] = (w[0] & 0xFFFFFFu) | (2u << 24);
  w[1] = get_mode(dfe, dtd, (u_short)tpage);
  w[2] = get_tw(tw);
}

/* ---- libgte ----------------------------------------------------------------- */

static short sat16(int v) {
  return v < -0x8000 ? -0x8000 : v > 0x7FFF ? 0x7FFF : (short)v;
}

/* m2 = m0 * m1 (rotation parts, 4.12 fixed point), as the GTE computes it:
 * each product sum shifted right by 12 and saturated to 16 bits. */
void Shim_MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2) {
  MATRIX r;
  int i, j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      int s = m0->m[i][0] * m1->m[0][j] + m0->m[i][1] * m1->m[1][j] +
              m0->m[i][2] * m1->m[2][j];
      r.m[i][j] = sat16(s >> 12);
    }
  }
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      m2->m[i][j] = r.m[i][j];
    }
  }
}

/* ---- libspu ----------------------------------------------------------------- */

void InitSpu(void) { SpuInit(); }

void ApplySpuVoiceAttr(SpuVoiceAttr *attr) { SpuSetVoiceAttr(attr); }

/* Voices keyed on since the SPU last ran (psyz_bridge.c clears it). */
unsigned int native_spu_fresh_keys;

void SetSpuKeyMask(int on, unsigned int voices) {
  if (on) {
    native_spu_fresh_keys |= voices;
  } else {
    native_spu_fresh_keys &= ~voices;
  }
  SpuSetKey(on, voices);
}

/* The SPU only runs when the host asks for samples (once per frame), but on
   PS1 it runs alongside the CPU: CdReadSyncSectors polls the game's voice
   tick while it waits for the disc, and by the next poll a voice keyed on
   has left its envelope's zero point. A natively instant read polls twice
   with no samples between, sees SPU_OFF_ENV_ON (keyed, envelope 0: how the
   tick spots a finished sound) and frees the voice. Until the SPU has run,
   a freshly keyed voice reads SPU_ON. */
int GetSpuVoiceKeyStatus(unsigned int voice_bit) {
  long status = SpuGetKeyStatus(voice_bit);

  if (status == SPU_OFF_ENV_ON && (voice_bit & native_spu_fresh_keys)) {
    status = SPU_ON;
  }
  return (int)status;
}

void GetAllSpuVoiceKeyStatus(char *status) { SpuGetAllKeysStatus(status); }

unsigned int ReadSpuRam(unsigned char *buf, unsigned int size) {
  return SpuRead(buf, size);
}

unsigned int WriteSpuRam(unsigned char *buf, unsigned int size) {
  return SpuWrite(buf, size);
}

int SetSpuTransferStartAddr(unsigned int addr) {
  return (int)SpuSetTransferStartAddr(addr);
}

void SetSpuTransferMode(int mode) { SpuSetTransferMode(mode); }

long GetSpuTransferStatus(int mode) { return SpuIsTransferCompleted(mode); }

void SetSpuCommonAttr(SpuCommonAttr *attr) { SpuSetCommonAttr(attr); }

void SetSpuVoiceVolumeLR(int voice, short l, short r) {
  SpuSetVoiceVolume(voice, l, r);
}

/* ---- libcd ------------------------------------------------------------------ */

/* PsyZ reads sectors synchronously, so the read is complete when CdRead
 * returns and the shim raises the CdlComplete callback right then: the PS1's
 * CD interrupt can arrive at any point after the call, and every game caller
 * raises its in-flight flag before calling CdRead (load/cd_stream.c). */
/* The game's handler takes the interrupt code only
 * (CdReadCompleteCallback(unsigned char)); a wasm indirect call must use the
 * callee's exact signature. */
typedef void (*GameCdReadCB)(unsigned char intr);
static GameCdReadCB cd_read_cb;

void *Shim_CdReadCallback(void *func) {
  GameCdReadCB prev = cd_read_cb;

  cd_read_cb = (GameCdReadCB)func;
  return (void *)prev;
}

/* libcd's last command byte (CdLastCom). PsyZ never records it, so the shim
 * tracks every command the game issues: TickCdMusicStream only trusts the
 * position in its CdSync result while the last command is CdlGetlocP. */
static unsigned char cd_last_com;

/* PsyZ does not implement CdlGetlocP. Its result is built from CdlGetlocL
 * (the XA read position while streaming) and handed back by the next
 * CdSync, as the PS1's interrupt would: the game reads only the absolute MSF
 * (bytes 5..7) to keep its music read head, which is where a resume after
 * the pause menu restarts the stream. Not CdControlB: its CdSync copies
 * GetlocL's cached status-only result over the position. */
static unsigned char cd_getlocp_result[8];

static void cd_track_com(unsigned char com) {
  unsigned char locl[8];

  if (com == CdlGetlocP && CdControl(CdlGetlocL, NULL, locl)) {
    cd_getlocp_result[0] = 1; /* track */
    cd_getlocp_result[1] = 1; /* index */
    cd_getlocp_result[2] = locl[0];
    cd_getlocp_result[3] = locl[1];
    cd_getlocp_result[4] = locl[2];
    cd_getlocp_result[5] = locl[0];
    cd_getlocp_result[6] = locl[1];
    cd_getlocp_result[7] = locl[2];
  }
  cd_last_com = com;
}

int Shim_CdRead(int sectors, u_long *buf, int mode) {
  int ok;

  cd_track_com(CdlReadN);
  ok = CdRead(sectors, buf, mode);

  if (cd_read_cb) {
    cd_read_cb(ok ? CdlComplete : CdlDiskError);
  }
  return ok;
}

int CallCdSync(int mode, unsigned char *result) {
  int ret = CdSync(mode, result);

  if (result != NULL && cd_last_com == CdlGetlocP) {
    memcpy(result, cd_getlocp_result, sizeof(cd_getlocp_result));
  }
  return ret;
}

int Shim_CdControl(unsigned char com, unsigned char *param,
                   unsigned char *result) {
  cd_track_com(com);
  return CdControl(com, param, result);
}

int CdControlBlocking(int com, unsigned char *param, unsigned char *result) {
  cd_track_com((unsigned char)com);
  return CdControlB(com, param, result);
}

int CdControlForce(unsigned char com, unsigned char *param) {
  cd_track_com(com);
  return CdControlF(com, param);
}

/* libcd's CdLastCom (0x80063aac, between CdMode and CdLastPos), not a
 * status byte. */
int GetCdStatusByte(void) { return cd_last_com; }

/* ---- libpad (direct mode) --------------------------------------------------- */

/* PsyZ's InitPAD fills the two raw report buffers on every VBlank, the way
 * libpad's direct mode does. */
#define PAD_REPORT_LEN 34

void PadInitDirect(unsigned char *report1, unsigned char *report2) {
  InitPAD((char *)report1, PAD_REPORT_LEN, (char *)report2, PAD_REPORT_LEN);
  StartPAD();
}

int PadSetMainMode(int socket, int offs, int lock) {
  (void)socket;
  (void)offs;
  (void)lock;
  return 1;
}

/* No actuator / mode tables natively: report a pad with none. */
int PadInfoMode(int socket, int term, int offs) {
  (void)socket;
  (void)term;
  (void)offs;
  return 0;
}

void PadStopCom(void) {}

