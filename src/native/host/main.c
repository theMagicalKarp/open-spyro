/* src/native/host/main.c — the native executable's entry point.
 *
 *   spyro --disc game.cue [--frames N] [--screenshot out.bmp]
 *         [--capture out.bmp] [--shot N]... [--resolution-scale N] [--aspect A]
 *         [--press N:BUTTONS[:HOLD]]... [--warp N:LEVEL]... [--fast]
 *         [--wav out.wav] [--memcard card.mcd] [--memcard2 card.mcd]
 *         [--log-level debug|info|warn|error]
 *   spyro --stub [--frames N] [--screenshot out.bmp]
 *
 * plus [--set KEY=VALUE]... and [[--config file.toml]... | --no-config] on
 * either form; --disc may instead come from a config file.
 *
 * --disc is the user's disc image (.cue + its .bin); the game's executable
 * and data are read from it. --stub instead runs the stub test scene (a
 * spinning triangle through PsyZ), which needs no disc. --frames ends the run
 * after N VSyncs; --screenshot then writes all of VRAM (1024x512, both display
 * buffers) as a BMP, for checking a run headlessly; --capture writes the
 * displayed frame as the window shows it (at the scaled resolution, and the
 * whole canvas in PsyZ's native mode). --shot N also writes them at VSync N,
 * next to each path (out-N.bmp). --resolution-scale N is short for
 * --set video.resolution_scale=N, --aspect A for --set video.aspect=A
 * (4:3, 16:9, 21:9, any W:H up to 8:3, or auto for the window's);
 * --resolution-scale takes 1..8 or auto. --press holds pad 1 buttons (BUTTONS:
 * names joined by '+', e.g. start, cross+up) for HOLD VSyncs (default 4) from
 * VSync N, on top of the keyboard / gamepad. --warp starts a load of level id
 * LEVEL (10 = Artisans, 11 = Stone Hill, ...) at VSync N, or as soon as
 * gameplay resumes after it. --fast runs without the 59.94 Hz frame limiter.
 * --wav writes the sound output to a WAV file as well.
 * --memcard / --memcard2 insert a raw 128 KiB card image (.mcd, created on
 * the first save) in port A / B; port A defaults to card1.mcd in the user's
 * data directory, "none" leaves the port empty. --log-level takes the word
 * or its first letter.
 *
 * The persistent settings have dotted keys (disc, memcard.port1/port2,
 * debug.fast, debug.log_level, debug.overlay, debug.start.level (boot
 * straight into that level id with a fresh game, showing nothing before it is
 * in play), video.resolution_scale, video.aspect, video.hud; the settings
 * table in config.c).
 * They can come from TOML config files: config.toml in the user's data
 * directory if it exists, or each --config PATH in order (later files override
 * earlier ones; each must exist); --no-config reads none. Relative paths in a
 * file are relative to it, and unknown keys are warned about and ignored. Flags
 * override the files: --set KEY=VALUE sets any key, and --disc, --memcard,
 * --memcard2, --fast and --log-level are short forms of it.
 *
 * Sets up the PS1 address space (psx_memory.c), instantiates the wasm2c'd
 * module (game C + PsyZ core) and runs it on this thread. PsyZ's platform
 * layer (SDL3 window, GPU backend, pads) runs natively on the host side of the
 * import boundary (imports.c). */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "config.h"
#include "diag.h"
#include "native_host.h"
#include "script.h"
#include "stick.h"
#include "tomlc17.h"

/* PsyZ's psyz/video.h and psyz/types.h (its include/ shadows system headers,
   so it stays off this file's path). */
typedef struct {
  int w, h;
} PsyzSize;
typedef struct {
  int sx, sy, ex, ey;
} PsyzRect;
int Psyz_VideoSetVsyncMode(int mode);
int Psyz_VideoVSync(int mode);
/* PsyZ's src/draw.h: GP1 display enable (hidden frames keep it off). */
void Draw_DisplayEnable(unsigned int on);
enum { PSYZ_VSYNC_AUTO = 0, PSYZ_VSYNC_LIMITLESS = 3 };
int Psyz_VideoSetResolution(PsyzSize size);
int Psyz_VideoSimulateDisplay(PsyzSize size);
int Psyz_VideoSetAnchor(int anchor);
enum { PSYZ_ANCHOR_CENTER = 0 };
int Psyz_VideoSetInternalResolution(unsigned multiplier);
unsigned Psyz_VideoGetInternalResolution(void);
unsigned char *Psyz_VideoAllocCapturedFrame(int *w, int *h);
PsyzRect Psyz_VideoGetDrawArea(void);
/* psyz/input.h's PsyzControllerKind: the pad ids PsyZ reports in byte 1. */
enum { PSYZ_CTRL_DIGITAL_PAD = 0x41, PSYZ_CTRL_ANALOG_PAD = 0x73 };

static struct w2c_env env;
static struct w2c_host host;
static w2c_spyro instance;
static int frame_limit;
static int frames_run;
static const char *screenshot_path;
static const char *capture_path;
static int run_stub;

enum { MAX_SCRIPT = 64 };
static int shot_frames[MAX_SCRIPT], n_shots;
static ScriptPress presses[MAX_SCRIPT];
static int n_presses;
static struct {
  int frame, level;
} warps[MAX_SCRIPT];
static int n_warps;
static NativeSettings cfg = NATIVE_SETTINGS_DEFAULT;
/* debug.start.level: from boot until the level is in play (1 until the
   title hands off, 2 while the level loads) the run is --fast and nothing is
   shown or heard, and the story intro's fly-over is cut short, so the game
   opens on the level. */
static int skipping_intro;
/* Fast VSyncs a skip may take before it gives up and shows the run (a normal
   one takes a few hundred). */
enum { SKIP_INTRO_MAX_VSYNCS = 60 * 60 };
static int vsync_offset;
static char default_card[1024];

/* --- import-module context accessors (wasm2c calls these at instantiate) ---
 */

wasm_rt_memory_t *w2c_env_memory(struct w2c_env *e) { return &e->memory; }
wasm_rt_funcref_table_t *
w2c_env_0x5F_indirect_function_table(struct w2c_env *e) {
  return &e->table;
}
u32 *w2c_env_0x5F_memory_base(struct w2c_env *e) { return &e->memory_base; }
u32 *w2c_env_0x5F_table_base(struct w2c_env *e) { return &e->table_base; }
u32 *w2c_env_0x5F_stack_pointer(struct w2c_env *e) { return &e->stack_pointer; }

wasm_rt_memory_t *native_memory(void) { return &env.memory; }
w2c_spyro *native_instance(void) { return &instance; }
int native_log_level(void) { return cfg.log_level; }
int native_fast(void) { return cfg.fast || skipping_intro; }
int native_vsync_offset(void) { return vsync_offset; }
int native_hiding_frames(void) { return skipping_intro != 0; }

/* Port A defaults to card1.mcd in the per-user data directory
   (~/Library/Application Support/open-spyro/spyro/); "none" removes it. */
const char *native_card_path(int port) {
  const char *p = cfg.card[port];

  if (port == 0 && p == NULL) {
    char *dir = SDL_GetPrefPath("open-spyro", "spyro");

    if (dir == NULL) {
      return NULL;
    }
    snprintf(default_card, sizeof(default_card), "%scard1.mcd", dir);
    SDL_free(dir);
    p = cfg.card[0] = default_card;
  }
  return p == NULL || strcmp(p, "none") == 0 ? NULL : p;
}
int native_frames_run(void) { return frames_run; }

/* The canvas PsyZ's native mode is asked for, or 0x0 for its PS1 mode. */
static PsyzSize video_request;

/* PsyZ's native mode keeps an upload over the framebuffer inside its canvas,
   and a 24-bit image (the boot splash screens) is 1.5 times the framebuffer's
   width in VRAM: its right third would be lost. PsyZ presents 24-bit images
   from VRAM anyway, so its PS1 mode stands in while one is displayed. */
static int showing_rgb24;
void native_display_mode(int rgb24) {
  showing_rgb24 = rgb24;
  Psyz_VideoSetResolution(rgb24 ? (PsyzSize){0, 0} : video_request);
}

/* The view's width over its height: 4:3 in PsyZ's PS1 mode, which also
   stands in while a 24-bit boot image is shown. */
double native_view_aspect(void) {
  return video_request.w > 0 && !showing_rgb24 ? video_request.w / 240.0
                                               : 4.0 / 3.0;
}

/* The window's size in pixels: asked for once it exists, then kept up to
   date by its resize events (native_window_resized), so the per-frame auto
   settings don't query SDL every frame. */
static int window_w, window_h;
void native_window_resized(int w, int h) {
  if (w > 0 && h > 0) {
    window_w = w;
    window_h = h;
  }
}

static int window_size(int *w, int *h) {
  if (window_w == 0) {
    int count;
    SDL_Window **windows = SDL_GetWindows(&count);

    if (windows != NULL && count > 0 &&
        SDL_GetWindowSizeInPixels(windows[0], w, h)) {
      native_window_resized(*w, *h);
    }
    SDL_free(windows);
  }
  *w = window_w;
  *h = window_h;
  return window_w > 0;
}

/* Native mode at WIDTH x 240 at the display aspect (320 is 4:3: a 512x240
   canvas, the framebuffer's own size; wider is more of the world at the
   sides, the framebuffer centred in it). Native mode presents at whole
   multiples of the display it is given (and fits that to the window the
   same way), leaving borders in most windows; told the display is 32 times
   the request, larger than any window, it scales the frame to fit the
   window at the canvas's aspect. */
static void set_video_request(int width) {
  if (video_request.w == width) {
    return;
  }
  video_request = (PsyzSize){width, 240};
  Psyz_VideoSimulateDisplay(
      (PsyzSize){video_request.w * 32, video_request.h * 32});
  if (!showing_rgb24) {
    Psyz_VideoSetResolution(video_request);
  }
}

/* video.aspect = auto: the window's aspect, within the range PsyZ can draw.
   Applied here, between frames. */
static void update_auto_aspect(void) {
  int w, h;

  if (window_size(&w, &h)) {
    set_video_request(config_aspect_request_width((double)w / h));
  }
}

/* video.resolution_scale = "auto": enough scale for the window's height, or
   for its width at the view's aspect, so the fit to the window shrinks
   rather than blurs. */
static void update_auto_resolution_scale(void) {
  int w, h, fit;

  if (!window_size(&w, &h)) {
    return;
  }
  fit = (int)(w / native_view_aspect());
  fit = h < fit ? h : fit;
  fit = (fit + 239) / 240;
  Psyz_VideoSetInternalResolution(fit < 1 ? 1 : fit > 8 ? 8 : (unsigned)fit);
}

/* F5 (diag.cpp): the next resolution scale in 1, 2, 4, 8, taken over from
   auto. Applied by the next frame hook, between frames; from PsyZ's PS1
   mode (1x at 4:3) it first switches to its native mode, where it scales. */
static unsigned pending_resolution_scale;
void native_cycle_resolution_scale(void) {
  unsigned cur = Psyz_VideoGetInternalResolution(), next = 1;

  while (next <= cur && next < 8) {
    next *= 2;
  }
  pending_resolution_scale = cur >= 8 ? 1 : next;
}

static void apply_pending_resolution_scale(void) {
  if (pending_resolution_scale == 0) {
    return;
  }
  if (video_request.w == 0) {
    Psyz_VideoSetAnchor(PSYZ_ANCHOR_CENTER);
    set_video_request(320);
  }
  cfg.resolution_scale = (int)pending_resolution_scale;
  Psyz_VideoSetInternalResolution(pending_resolution_scale);
  pending_resolution_scale = 0;
}

static void write_screenshot(const char *path);
static void write_capture(const char *path);

/* The skip is over: frames are shown again and, unless --fast, the run goes
   back to real time with VSync(-1) carrying on from the VSync count. */
static void end_skip_intro(void) {
  skipping_intro = 0;
  Draw_DisplayEnable(1);
  if (!cfg.fast) {
    vsync_offset = frames_run - Psyz_VideoVSync(-1);
    Psyz_VideoSetVsyncMode(PSYZ_VSYNC_AUTO);
  }
}

int native_frame_hook(void) {
  static unsigned logged_res;
  static PsyzRect logged_area;

  frames_run++;
  if (cfg.aspect == 0) {
    update_auto_aspect();
  }
  apply_pending_resolution_scale();
  if (cfg.resolution_scale == 0) {
    update_auto_resolution_scale();
  }
  if (Psyz_VideoGetInternalResolution() != logged_res) {
    logged_res = Psyz_VideoGetInternalResolution();
    printf("native: resolution scale %ux\n", logged_res);
  }
  if (video_request.w > 0 && !showing_rgb24) {
    PsyzRect a = Psyz_VideoGetDrawArea();

    if (a.sx != logged_area.sx || a.ex != logged_area.ex ||
        a.sy != logged_area.sy || a.ey != logged_area.ey) {
      logged_area = a;
      printf("native: view %dx%d, x %d..%d y %d..%d\n", a.ex - a.sx,
             a.ey - a.sy, a.sx, a.ex, a.sy, a.ey);
      /* The game's culls and full-screen draws widen to it (native_view.h),
         from the next frame on. Only its 512-wide framebuffer is wider than
         4:3 (the boot screens' are narrower, and draw nothing to widen).
         debug.cull_margin only reaches past a widened view's edges: at 4:3
         the view stays the framebuffer, as the original's. */
      if (a.sx <= 0 && a.ex >= 512) {
        int margin = a.sx < 0 ? cfg.cull_margin : 0;

        if (a.sx + a.ex != 512) {
          /* native_view.h takes the view as centred on the framebuffer (the
             culls widen by its half-width from x = 256). */
          fprintf(stderr, "native: view x %d..%d is not centred on 0..512\n",
                  a.sx, a.ex);
        }
        w2c_spyro_native_set_view(&instance, (u32)a.sx, (u32)a.ex, (u32)margin);
      }
    }
  }
  for (int i = 0; i < n_shots; i++) {
    if (shot_frames[i] == frames_run) {
      char path[1024];

      if (screenshot_path != NULL) {
        script_shot_path(path, sizeof(path), screenshot_path, frames_run);
        write_screenshot(path);
      }
      if (capture_path != NULL) {
        script_shot_path(path, sizeof(path), capture_path, frames_run);
        write_capture(path);
      }
    }
  }
  for (int i = 0; i < n_warps; i++) {
    if (warps[i].frame != 0 && frames_run >= warps[i].frame &&
        w2c_spyro_native_warp(&instance, (u32)warps[i].level) != 0) {
      printf("native: warp to level %d at VSync %d\n", warps[i].level,
             frames_run);
      warps[i].frame = 0;
    }
  }
  if (skipping_intro == 1 &&
      w2c_spyro_native_start_level(&instance, (u32)cfg.start_level) != 0) {
    printf("native: start level %d at VSync %d\n", cfg.start_level, frames_run);
    skipping_intro = 2;
  } else if (skipping_intro == 2) {
    w2c_spyro_native_skip_intro_fly(&instance);
  }
  if (skipping_intro == 2 && w2c_spyro_native_in_gameplay(&instance)) {
    printf("native: level %d in play at VSync %d\n", cfg.start_level,
           frames_run);
    end_skip_intro();
  } else if (skipping_intro != 0 && frames_run >= SKIP_INTRO_MAX_VSYNCS) {
    fprintf(stderr,
            "native: debug.start.level %d not in play after %d VSyncs; "
            "showing the run\n",
            cfg.start_level, frames_run);
    end_skip_intro();
  }
  return frame_limit != 0 && frames_run >= frame_limit;
}

/* PsyZ backend (sdl3_gpu.c); RECT is 4 shorts, pixels are RGB555. */
typedef struct {
  short x, y, w, h;
} VramRect;
void Draw_StoreImage(VramRect *rect, unsigned long *p);
int Draw_ExequeSync(void);

static void put32(FILE *f, unsigned v) {
  unsigned char b[4] = {v, v >> 8, v >> 16, v >> 24};

  fwrite(b, 1, 4, f);
}

/* A 24-bit top-down BMP of W x H pixels, RGB888 rows of RGB. */
static void write_bmp(const char *path, int w, int h,
                      const unsigned char *rgb) {
  unsigned stride = ((unsigned)w * 3 + 3) & ~3u;
  static const unsigned char pad[3];
  FILE *f = fopen(path, "wb");

  if (f == NULL) {
    perror(path);
    return;
  }
  fwrite("BM", 1, 2, f);
  put32(f, 54 + stride * h);
  put32(f, 0);
  put32(f, 54);
  put32(f, 40);
  put32(f, w);
  put32(f, (unsigned)-h); /* top-down rows */
  put32(f, 1 | (24 << 16));
  put32(f, 0);
  put32(f, stride * h);
  put32(f, 2835);
  put32(f, 2835);
  put32(f, 0);
  put32(f, 0);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      const unsigned char *p = &rgb[((size_t)y * w + x) * 3];
      unsigned char bgr[3] = {p[2], p[1], p[0]};

      fwrite(bgr, 1, 3, f);
    }
    fwrite(pad, 1, stride - (unsigned)w * 3, f);
  }
  fclose(f);
  printf("native: wrote %s (%dx%d)\n", path, w, h);
}

static void write_screenshot(const char *path) {
  enum { W = 1024, H = 512 };
  static unsigned short vram[W * H];
  static unsigned char rgb[W * H * 3];
  VramRect r = {0, 0, W, H};

  Draw_StoreImage(&r, (unsigned long *)vram);
  Draw_ExequeSync();
  for (int i = 0; i < W * H; i++) {
    unsigned p = vram[i];

    rgb[i * 3 + 0] = (p & 31) << 3;
    rgb[i * 3 + 1] = ((p >> 5) & 31) << 3;
    rgb[i * 3 + 2] = ((p >> 10) & 31) << 3;
  }
  write_bmp(path, W, H, rgb);
}

static void write_capture(const char *path) {
  int w, h;
  unsigned char *rgb = Psyz_VideoAllocCapturedFrame(&w, &h);

  if (rgb == NULL) {
    fprintf(stderr, "native: cannot capture the frame for %s\n", path);
    return;
  }
  write_bmp(path, w, h, rgb);
  free(rgb);
}

_Noreturn void native_quit(int code) {
  if (screenshot_path != NULL) {
    write_screenshot(screenshot_path);
  }
  if (capture_path != NULL) {
    write_capture(capture_path);
  }
  native_audio_close();
  printf("native: exit %d after %d frames\n", code, frames_run);
  fflush(stdout);
  SDL_Quit();
  exit(code);
}

/* --- core functions PsyZ's backend calls, forwarded into the module ------- */

void Psyz_PadsSet(int port, const char *src, int len) {
  u32 buf = w2c_spyro_native_bridge_buffer(&instance);
  unsigned char *dst = psx_ptr(&env.memory, buf);

  if (len > 256) {
    len = 256;
  }
  memcpy(dst, src, (size_t)len);
  stick_shape_report(dst, len, &cfg.stick[0], &cfg.stick[1]);
  if (port == 0) {
    script_apply_pad(dst, len, script_buttons(presses, n_presses, frames_run));
  }
  w2c_spyro_native_pads_set(&instance, (u32)port, (u32)len);
}

int Psyz_PadsSetKind(int port, int channel, int kind) {
  return (int)w2c_spyro_native_pads_set_kind(&instance, (u32)port, (u32)channel,
                                             (u32)kind);
}

/* The backend's 24-bit display path (AllocRgb24Region) reads VRAM back
   through libgpu with host buffers: the boot splash screens are 24-bit. */
int DrawSync(int mode) {
  (void)mode;
  return Draw_ExequeSync();
}
int StoreImage(void *rect, void *p) {
  Draw_StoreImage(rect, p);
  return 0;
}

static void usage(void) {
  fprintf(stderr, "usage: spyro [--disc game.cue | --stub] [--frames N] "
                  "[--screenshot out.bmp] [--capture out.bmp] [--shot N]... "
                  "[--resolution-scale N] [--aspect 4:3|16:9|21:9|W:H|auto] "
                  "[--press N:BUTTONS[:HOLD]]... [--warp N:LEVEL]... "
                  "[--fast] [--wav out.wav] [--memcard card.mcd] "
                  "[--memcard2 card.mcd] [--log-level debug|info|warn|error] "
                  "[--set KEY=VALUE]... "
                  "[[--config file.toml]... | --no-config]\n"
                  "(--disc is needed unless a config file sets disc)\n");
  exit(2);
}

/* --set and the short flags: a bad key or value ends the run. */
static void set_from_flag(const char *flag, const char *key,
                          const char *value) {
  if (config_set(&cfg, flag, key, value) != 0) {
    exit(2);
  }
}

/* config.toml in the user's data directory, when it exists. */
static void load_default_config(void) {
  static char path[1024];
  char *dir = SDL_GetPrefPath("open-spyro", "spyro");

  if (dir == NULL) {
    return;
  }
  snprintf(path, sizeof(path), "%sconfig.toml", dir);
  SDL_free(dir);
  if (config_load_file(&cfg, path, 1) != 0) {
    exit(2);
  }
}

int main(int argc, char **argv) {
  u32 table_size;
  int ret;

  setvbuf(stdout, NULL, _IOLBF, 0);
  /* The config files first, in order, so the flags below override them. */
  {
    const char *paths[CONFIG_MAX_FILES];
    int n, none;

    if (config_scan_args(argc, argv, paths, &n, &none) != 0) {
      usage();
    }
    if (n == 0 && !none) {
      load_default_config();
    }
    for (int i = 0; i < n; i++) {
      if (config_load_file(&cfg, paths[i], 0) != 0) {
        exit(2);
      }
    }
  }
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      i++; /* read above */
    } else if (strcmp(argv[i], "--no-config") == 0) {
    } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
      frame_limit = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--set") == 0 && i + 1 < argc) {
      char *key = argv[++i], *eq = strchr(key, '=');

      if (eq == NULL) {
        usage();
      }
      *eq = '\0';
      set_from_flag("--set", key, eq + 1);
    } else if (strcmp(argv[i], "--disc") == 0 && i + 1 < argc) {
      set_from_flag("--disc", "disc", argv[++i]);
    } else if (strcmp(argv[i], "--stub") == 0) {
      run_stub = 1;
    } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
      screenshot_path = argv[++i];
    } else if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc) {
      capture_path = argv[++i];
    } else if (strcmp(argv[i], "--resolution-scale") == 0 && i + 1 < argc) {
      set_from_flag("--resolution-scale", "video.resolution_scale", argv[++i]);
    } else if (strcmp(argv[i], "--aspect") == 0 && i + 1 < argc) {
      set_from_flag("--aspect", "video.aspect", argv[++i]);
    } else if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc &&
               n_shots < MAX_SCRIPT) {
      shot_frames[n_shots++] = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--press") == 0 && i + 1 < argc &&
               n_presses < MAX_SCRIPT) {
      switch (config_parse_press(argv[++i], &presses[n_presses].frame,
                                 &presses[n_presses].buttons,
                                 &presses[n_presses].hold)) {
      case 0:
        break;
      case 1:
        usage();
      default:
        exit(2);
      }
      n_presses++;
    } else if (strcmp(argv[i], "--warp") == 0 && i + 1 < argc &&
               n_warps < MAX_SCRIPT) {
      if (config_parse_warp(argv[++i], &warps[n_warps].frame,
                            &warps[n_warps].level) != 0) {
        usage();
      }
      n_warps++;
    } else if (strcmp(argv[i], "--fast") == 0) {
      set_from_flag("--fast", "debug.fast", "true");
    } else if (strcmp(argv[i], "--memcard") == 0 && i + 1 < argc) {
      set_from_flag("--memcard", "memcard.port1", argv[++i]);
    } else if (strcmp(argv[i], "--memcard2") == 0 && i + 1 < argc) {
      set_from_flag("--memcard2", "memcard.port2", argv[++i]);
    } else if (strcmp(argv[i], "--wav") == 0 && i + 1 < argc) {
      if (native_audio_wav(argv[++i]) != 0) {
        fprintf(stderr, "native: cannot write %s\n", argv[i]);
        return 1;
      }
    } else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
      set_from_flag("--log-level", "debug.log_level", argv[++i]);
    } else {
      usage();
    }
  }

  if ((!run_stub && cfg.disc == NULL) ||
      (n_shots > 0 && screenshot_path == NULL && capture_path == NULL)) {
    usage();
  }
  if (!run_stub) {
    FILE *f = fopen(cfg.disc, "rb");

    if (f == NULL) {
      if (cfg.disc_file != NULL) {
        fprintf(stderr, "native: %s: cannot read disc image %s\n",
                cfg.disc_file, cfg.disc);
      } else {
        fprintf(stderr, "native: cannot read disc image %s\n", cfg.disc);
      }
      return 2;
    }
    fclose(f);
  }
  wasm_rt_init();
  if (psx_memory_init(&env.memory) != 0) {
    return 1;
  }
  table_size =
      NATIVE_TABLE_BASE + wasm2c_spyro_min_env_0x5F_indirect_function_table;
  if (table_size < NATIVE_TABLE_SIZE) {
    table_size = NATIVE_TABLE_SIZE;
  }
  wasm_rt_allocate_funcref_table(&env.table, table_size, table_size);
  env.memory_base = NATIVE_MODULE_BASE;
  env.table_base = NATIVE_TABLE_BASE;
  env.stack_pointer = NATIVE_STACK_TOP;

  /* GOT.mem imports resolve to PS1 addresses (spyro_w_got.c) and need no
     context. */
  diag_init(cfg.overlay);
  skipping_intro = !run_stub && cfg.start_level != 0;
  if (native_fast()) {
    /* Applied when the backend configures VSync at init. */
    Psyz_VideoSetVsyncMode(PSYZ_VSYNC_LIMITLESS);
  }
  /* Stored until the backend exists, then applied on each Present. PsyZ only
     scales in its native mode (a canvas of its own, which capture reads), and
     only draws wider than 4:3 there, so anything but 1x at 4:3 asks for one.
     Its own auto scale would then always be 1x (set_video_request), so auto
     (0) is worked out here from the window. */
  Psyz_VideoSetInternalResolution(
      cfg.resolution_scale == 0 ? 1 : (unsigned)cfg.resolution_scale);
  if (cfg.resolution_scale != 1 || cfg.aspect == 0 ||
      config_aspect_request_width(cfg.aspect) != 320) {
    Psyz_VideoSetAnchor(PSYZ_ANCHOR_CENTER);
    set_video_request(
        cfg.aspect == 0 ? 320 : config_aspect_request_width(cfg.aspect));
  }
  wasm2c_spyro_instantiate(&instance, NULL, &env, &host);
  w2c_spyro_0x5F_wasm_apply_data_relocs(&instance);
  w2c_spyro_0x5F_wasm_call_ctors(&instance);
  /* A DualShock in analog mode: the game calibrates the sticks from its first
     analog report and steers Spyro with the left one. */
  Psyz_PadsSetKind(0, 0,
                   cfg.analog ? PSYZ_CTRL_ANALOG_PAD : PSYZ_CTRL_DIGITAL_PAD);
  /* Not in the original game: the right stick's X axis turns the camera
     like L2/R2 (src/native/game/camera/manual_yaw.c), its rates in Q8. */
  w2c_spyro_native_set_hud(&instance, (u32)cfg.hud_edges);
  w2c_spyro_native_set_camera_stick(
      &instance, (u32)cfg.camera_analog, (u32)cfg.camera_invert,
      (u32)lround(cfg.camera_speed * 256), (u32)lround(cfg.camera_boost * 256),
      (u32)lround(cfg.camera_boost_time * 60 /* VSyncs */));

  if (run_stub) {
    ret = (int)w2c_spyro_native_stub_main(&instance, 0);
  } else {
    u32 arg = w2c_spyro_native_arg_buffer(&instance);

    if (strlen(cfg.disc) >= 1024) {
      fprintf(stderr, "native: disc path too long\n");
      return 2;
    }
    strcpy(psx_ptr(&env.memory, arg), cfg.disc);
    ret = (int)w2c_spyro_native_main(&instance);
  }
  native_quit(ret);
}
