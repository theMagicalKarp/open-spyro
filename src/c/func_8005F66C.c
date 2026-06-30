#include "globals.h"

extern char D_80011834[]; /* GPU debug format string */

/* Install the GPU queue-completion callback and return the previous one
   (libgpu DrawSyncCallback shape). When the GPU debug level is >= 2 the change
   is traced through g_pfnGpuDebugPrintf first. (0x8005f66c, 92 bytes.) */
void *func_8005F66C(void *callback) {
  void *old;
  if (g_bGpuDebugLevel >= 2) {
    ((void (*)(char *, void *))g_pfnGpuDebugPrintf)(D_80011834, callback);
  }
  old = g_pfnGpuQueueCompletionCallback;
  g_pfnGpuQueueCompletionCallback = callback;
  return old;
}
