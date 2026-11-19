/* src/native/wasm/psyz_bridge.c — the PsyZ platform functions that live on
 * the wasm side of the import boundary.
 *
 * Single-threaded: a wasm2c instance must only ever run on one thread, so
 * Psyz_OsThreadStart fails and PsyZ's kernel falls back to polling its timers
 * from VSync (kernel.c, Psyz_KernelPoll). Locks are then no-ops. Audio is
 * pushed, not pulled: the host asks for the SPU's output between frames
 * (native_audio_pull) and queues it on an SDL audio stream, so no SDL audio
 * thread ever enters the module. The memory-card directory API is a stub
 * until 5.5. Pad input crosses as an export the host calls
 * (psyz_bridge_pads_set).
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <psyz.h>
#include <psyz/input.h>
#include <psyz/spu.h>

#include "host_imports.h"

#define EXPORT(name) __attribute__((export_name(#name)))

void psyz_log(unsigned int level, const char *file, unsigned int line,
              const char *func, const char *fmt, ...) {
  char msg[1024];
  int n;
  va_list ap;

  n = snprintf(msg, sizeof(msg), "[%s:%u][%s] ", file, line, func);
  va_start(ap, fmt);
  vsnprintf(msg + n, sizeof(msg) - (unsigned)n, fmt, ap);
  va_end(ap);
  host_log((int)level, msg);
}

/* --- os.h: one thread, no blocking --------------------------------------- */

uint32_t Psyz_OsNowUs(void) { return host_now_us(); }
uintptr_t Psyz_OsThreadSelf(void) { return 1; }
void *Psyz_OsLockCreate(void) { return (void *)1; }
void Psyz_OsLock(void *lock) { (void)lock; }
void Psyz_OsUnlock(void *lock) { (void)lock; }
void *Psyz_OsBellCreate(void) { return (void *)1; }
void Psyz_OsBellRing(void *bell) { (void)bell; }
void Psyz_OsBellWait(void *bell, uint32_t timeout_us) {
  (void)bell;
  (void)timeout_us;
}
int Psyz_OsThreadStart(void (*fn)(void), const char *name) {
  (void)fn;
  (void)name;
  return -1;
}

/* --- audio.h: the host drives output ------------------------------------- */

/* libcd calls this when the game sets CD-DA / XA mode. The SPU emulation is
   already up (native_main starts it, since a first init clears SPU RAM), so
   this does nothing; the output device is the host's. */
int Psyz_AudioInit(void) {
  Psyz_SpuInit();
  return 0;
}
void Psyz_AudioLock(void) {}
void Psyz_AudioUnlock(void) {}

enum { AUDIO_PULL_MAX = 2048 };
static short audio_buf[AUDIO_PULL_MAX * 2];
extern unsigned int native_spu_fresh_keys; /* sdk_shim.c */

/* Runs the SPU for `frames` stereo sample frames (44.1 kHz; voices, reverb
   and CD / XA audio mixed) into a module buffer and returns its address. */
EXPORT(native_audio_pull) short *native_audio_pull(int frames) {
  if (frames > AUDIO_PULL_MAX) {
    frames = AUDIO_PULL_MAX;
  }
  Psyz_SpuPullSamples(audio_buf, frames);
  native_spu_fresh_keys = 0;
  return audio_buf;
}

/* --- memory-card directory API (libapi firstfile/nextfile/erase/format) --- */

struct DIRENTRY *my_firstfile(char *dirPath, struct DIRENTRY *firstEntry) {
  (void)dirPath;
  (void)firstEntry;
  return NULL;
}
struct DIRENTRY *my_nextfile(struct DIRENTRY *outEntry) {
  (void)outEntry;
  return NULL;
}
long my_erase(char *path) {
  (void)path;
  return 0;
}
long my_format(char *fs) {
  (void)fs;
  return 0;
}

/* --- exports the host calls ----------------------------------------------- */

/* Host -> PsyZ pad state: the host copies the frame into the bridge buffer
   (native_bridge_buffer) and passes its length. */
static char bridge_buffer[256];

EXPORT(native_bridge_buffer) char *native_bridge_buffer(void) {
  return bridge_buffer;
}

EXPORT(native_pads_set) void native_pads_set(int port, int len) {
  Psyz_PadsSet(port, bridge_buffer, len);
}

EXPORT(native_pads_set_kind)
int native_pads_set_kind(int port, int channel, int kind) {
  return (int)Psyz_PadsSetKind(port, channel, (PsyzControllerKind)kind);
}
