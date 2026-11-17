/* src/native/host/imports.c — what the wasm32 module imports from the host.
 *
 *   module "env":  PsyZ's GPU backend (Draw_*, draw.h), VSync and pads. These
 *                  forward to PsyZ's own sdl3_gpu backend, compiled natively.
 *   module "host": libc services (src/native/wasm/host_imports.h).
 *
 * Every pointer argument is a wasm address: psx_ptr() turns it into a host
 * pointer. The one ABI difference that needs more than that is u_long, which
 * PsyZ makes pointer-sized: 32-bit on the wasm side, 64-bit in the natively
 * built backend. GPU packet words are therefore widened before the backend
 * parses them (Draw_PushPrim), and u_long bitfield structs are copied into a
 * zeroed host struct (Draw_SetTexpageMode). Image data is accessed as 16-bit
 * pixels by the backend and needs no conversion. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <SDL3/SDL.h>

#include "native_host.h"

/* PsyZ backend entry points (sdl3_gpu.c / sdl3_common.h, built natively). */
#include "../../../external/psyz/psyz/src/draw.h"
int Psyz_VideoVSync(int mode);
void Psyz_PadsPoll(void);
void MyPadInit(int mode);

#define PTR(addr) psx_ptr(native_memory(), (addr))

/* --- env: GPU backend ------------------------------------------------------ */

void w2c_env_Draw_Reset(struct w2c_env *e) {
  (void)e;
  Draw_Reset();
}
void w2c_env_Draw_DisplayEnable(struct w2c_env *e, u32 on) {
  (void)e;
  Draw_DisplayEnable(on);
}
void w2c_env_Draw_DisplayArea(struct w2c_env *e, u32 x, u32 y) {
  (void)e;
  Draw_DisplayArea(x, y);
}
void w2c_env_Draw_DisplayHorizontalRange(struct w2c_env *e, u32 a, u32 b) {
  (void)e;
  Draw_DisplayHorizontalRange(a, b);
}
void w2c_env_Draw_DisplayVerticalRange(struct w2c_env *e, u32 a, u32 b) {
  (void)e;
  Draw_DisplayVerticalRange(a, b);
}
void w2c_env_Draw_SetDisplayMode(struct w2c_env *e, u32 mode) {
  (void)e;
  Draw_SetDisplayMode((DisplayMode *)PTR(mode)); /* unsigned int bitfields */
}
void w2c_env_Draw_SetTexpageMode(struct w2c_env *e, u32 p) {
  ParamDrawTexpageMode m;

  (void)e;
  memset(&m, 0, sizeof(m));
  memcpy(&m, PTR(p), 4); /* u_long bitfields: 4 bytes on wasm, 8 here */
  Draw_SetTexpageMode(&m);
}
void w2c_env_Draw_SetTextureWindow(struct w2c_env *e, u32 mx, u32 my, u32 ox,
                                   u32 oy) {
  (void)e;
  Draw_SetTextureWindow(mx, my, ox, oy);
}
void w2c_env_Draw_SetAreaStart(struct w2c_env *e, u32 x, u32 y) {
  (void)e;
  Draw_SetAreaStart((int)x, (int)y);
}
void w2c_env_Draw_SetAreaEnd(struct w2c_env *e, u32 x, u32 y) {
  (void)e;
  Draw_SetAreaEnd((int)x, (int)y);
}
void w2c_env_Draw_SetOffset(struct w2c_env *e, u32 x, u32 y) {
  (void)e;
  Draw_SetOffset((int)x, (int)y);
}
void w2c_env_Draw_SetMask(struct w2c_env *e, u32 a, u32 b) {
  (void)e;
  Draw_SetMask((int)a, (int)b);
}
void w2c_env_Draw_ClearImage(struct w2c_env *e, u32 rect, u32 r, u32 g,
                             u32 b) {
  (void)e;
  Draw_ClearImage((RECT *)PTR(rect), (u_char)r, (u_char)g, (u_char)b);
}
void w2c_env_Draw_LoadImage(struct w2c_env *e, u32 rect, u32 p) {
  (void)e;
  Draw_LoadImage((RECT *)PTR(rect), (u_long *)PTR(p));
}
void w2c_env_Draw_StoreImage(struct w2c_env *e, u32 rect, u32 p) {
  (void)e;
  Draw_StoreImage((RECT *)PTR(rect), (u_long *)PTR(p));
}
void w2c_env_Draw_MoveImage(struct w2c_env *e, u32 rect, u32 x, u32 y) {
  (void)e;
  Draw_MoveImage((RECT *)PTR(rect), x, y);
}
void w2c_env_Draw_ResetBuffer(struct w2c_env *e) {
  (void)e;
  Draw_ResetBuffer();
}
void w2c_env_Draw_FlushBuffer(struct w2c_env *e) {
  (void)e;
  Draw_FlushBuffer();
}
u32 w2c_env_Draw_ExequeSync(struct w2c_env *e) {
  (void)e;
  return (u32)Draw_ExequeSync();
}

/* The longest GPU command is a polyline (0x48/0x58), terminated by a
   0x5xxx5xxx word; no PS1 game sends one anywhere near this long. */
#define MAX_PRIM_WORDS 256

u32 w2c_env_Draw_PushPrim(struct w2c_env *e, u32 packets, u32 max_len) {
  u_long wide[MAX_PRIM_WORDS];
  const u32 *words = (const u32 *)PTR(packets);
  int n = (int)max_len < MAX_PRIM_WORDS ? (int)max_len : MAX_PRIM_WORDS;

  (void)e;
  for (int i = 0; i < n; i++) {
    wide[i] = words[i];
  }
  return (u32)Draw_PushPrim(wide, n);
}

/* --- env: frame pacing and input -------------------------------------------- */

u32 w2c_env_Psyz_VideoVSync(struct w2c_env *e, u32 mode) {
  (void)e;
  /* Only a blocking VSync is a frame: VSync(-1) / VSync(1) just read the
     counter. */
  if ((int)mode != -1 && mode != 1) {
    if (native_frame_hook() != 0) {
      native_quit(0);
    }
    native_audio_vsync();
  }
  if (native_fast() && (int)mode == -1) {
    return (u32)native_frames_run();
  }
  return (u32)Psyz_VideoVSync((int)mode);
}
void w2c_env_Psyz_PadsPoll(struct w2c_env *e) {
  (void)e;
  Psyz_PadsPoll();
}
void w2c_env_MyPadInit(struct w2c_env *e, u32 mode) {
  (void)e;
  MyPadInit((int)mode);
}

/* --- host: libc services ----------------------------------------------------- */

#define MAX_FILES 32
static FILE *files[MAX_FILES];

u32 w2c_host_write(struct w2c_host *h, u32 fd, u32 buf, u32 len) {
  (void)h;
  return (u32)fwrite(PTR(buf), 1, len, fd == 2 ? stderr : stdout);
}

void w2c_host_log(struct w2c_host *h, u32 level, u32 msg) {
  static const char levels[] = "DIWE";

  (void)h;
  if (level < 4 && (int)level >= native_log_level()) {
    fprintf(stderr, "[%c]%s\n", levels[level], (const char *)PTR(msg));
  }
}

void w2c_host_exit(struct w2c_host *h, u32 code) {
  (void)h;
  native_quit((int)code);
}

u32 w2c_host_now_us(struct w2c_host *h) {
  (void)h;
  return (u32)SDL_NS_TO_US(SDL_GetTicksNS());
}

u32 w2c_host_fopen(struct w2c_host *h, u32 path, u32 mode) {
  (void)h;
  for (int i = 0; i < MAX_FILES; i++) {
    if (files[i] == NULL) {
      files[i] = fopen((const char *)PTR(path), (const char *)PTR(mode));
      return files[i] ? (u32)i : (u32)-1;
    }
  }
  return (u32)-1;
}

static FILE *file(u32 handle) {
  return handle < MAX_FILES ? files[handle] : NULL;
}

u32 w2c_host_fclose(struct w2c_host *h, u32 handle) {
  FILE *f = file(handle);

  (void)h;
  if (f == NULL) {
    return (u32)-1;
  }
  files[handle] = NULL;
  return (u32)fclose(f);
}
u32 w2c_host_fread(struct w2c_host *h, u32 dst, u32 len, u32 handle) {
  FILE *f = file(handle);

  (void)h;
  return f ? (u32)fread(PTR(dst), 1, len, f) : 0;
}
u32 w2c_host_fwrite(struct w2c_host *h, u32 src, u32 len, u32 handle) {
  FILE *f = file(handle);

  (void)h;
  return f ? (u32)fwrite(PTR(src), 1, len, f) : 0;
}
u32 w2c_host_fseek(struct w2c_host *h, u32 handle, u32 off, u32 whence) {
  FILE *f = file(handle);

  (void)h;
  return f ? (u32)fseek(f, (long)(int)off, (int)whence) : (u32)-1;
}
u32 w2c_host_ftell(struct w2c_host *h, u32 handle) {
  FILE *f = file(handle);

  (void)h;
  return f ? (u32)ftell(f) : (u32)-1;
}
u32 w2c_host_file_size(struct w2c_host *h, u32 path) {
  struct stat st;

  (void)h;
  if (stat((const char *)PTR(path), &st) != 0) {
    return (u32)-1;
  }
  return (u32)st.st_size;
}
u32 w2c_host_mkdir(struct w2c_host *h, u32 path) {
  (void)h;
  return (u32)mkdir((const char *)PTR(path), 0755);
}
u32 w2c_host_card_path(struct w2c_host *h, u32 port, u32 buf, u32 len) {
  const char *path = native_card_path((int)port);

  (void)h;
  if (path == NULL || strlen(path) >= len) {
    return 0;
  }
  strcpy((char *)PTR(buf), path);
  return 1;
}
