/* src/native/host/main.c — the native executable's entry point.
 *
 *   spyro --disc game.cue [--frames N] [--screenshot out.bmp] [--shot N]...
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
 * buffers) as a BMP, for checking a run headlessly. --shot N also writes one
 * at VSync N, next to --screenshot (out-N.bmp). --press holds pad 1 buttons
 * (BUTTONS: names joined by '+', e.g. start, cross+up) for HOLD VSyncs
 * (default 4) from VSync N, on top of the keyboard / gamepad. --warp starts
 * a load of level id LEVEL (10 = Artisans, 11 = Stone Hill, ...) at VSync N,
 * or as soon as gameplay resumes after it. --fast runs without the 59.94 Hz
 * frame limiter. --wav writes the sound output to a WAV file as well.
 * --memcard / --memcard2 insert a raw 128 KiB card image (.mcd, created on
 * the first save) in port A / B; port A defaults to card1.mcd in the user's
 * data directory, "none" leaves the port empty. --log-level takes the word
 * or its first letter.
 *
 * The persistent settings have dotted keys (disc, memcard.port1/port2,
 * debug.fast, debug.log_level; the settings table below). They can come
 * from TOML config files: config.toml in the user's data directory if it
 * exists, or each --config PATH in order (later files override earlier
 * ones; each must exist); --no-config reads none. Relative paths in a file
 * are relative to it, and unknown keys are warned about and ignored. Flags
 * override the files: --set KEY=VALUE sets any key, and --disc, --memcard,
 * --memcard2, --fast and --log-level are short forms of it.
 *
 * Sets up the PS1 address space (psx_memory.c), instantiates the wasm2c'd
 * module (game C + PsyZ core) and runs it on this thread. PsyZ's platform
 * layer (SDL3 window, GPU backend, pads) runs natively on the host side of the
 * import boundary (imports.c). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "native_host.h"
#include "tomlc17.h"

/* PsyZ's psyz/video.h (its include/ shadows system headers, so it stays off
   this file's path): PsyzVsyncMode PSYZ_VSYNC_LIMITLESS. */
int Psyz_VideoSetVsyncMode(int mode);
enum { PSYZ_VSYNC_LIMITLESS = 3 };

static struct w2c_env env;
static struct w2c_host host;
static w2c_spyro instance;
static int frame_limit;
static int frames_run;
static int log_level = 1; /* info */
static const char *screenshot_path;
static const char *disc_path;
static int run_stub;

enum { MAX_SCRIPT = 64 };
static int shot_frames[MAX_SCRIPT], n_shots;
static struct {
  int frame, hold;
  unsigned buttons;
} presses[MAX_SCRIPT];
static int n_presses;
static struct {
  int frame, level;
} warps[MAX_SCRIPT];
static int n_warps;
static int run_fast;
static const char *card_paths[2];
static char default_card[1024];

/* --- import-module context accessors (wasm2c calls these at instantiate) --- */

wasm_rt_memory_t *w2c_env_memory(struct w2c_env *e) { return &e->memory; }
wasm_rt_funcref_table_t *
w2c_env_0x5F_indirect_function_table(struct w2c_env *e) {
  return &e->table;
}
u32 *w2c_env_0x5F_memory_base(struct w2c_env *e) { return &e->memory_base; }
u32 *w2c_env_0x5F_table_base(struct w2c_env *e) { return &e->table_base; }
u32 *w2c_env_0x5F_stack_pointer(struct w2c_env *e) {
  return &e->stack_pointer;
}

wasm_rt_memory_t *native_memory(void) { return &env.memory; }
w2c_spyro *native_instance(void) { return &instance; }
int native_log_level(void) { return log_level; }
int native_fast(void) { return run_fast; }

/* Port A defaults to card1.mcd in the per-user data directory
   (~/Library/Application Support/open-spyro/spyro/); "none" removes it. */
const char *native_card_path(int port) {
  const char *p = card_paths[port];

  if (port == 0 && p == NULL) {
    char *dir = SDL_GetPrefPath("open-spyro", "spyro");

    if (dir == NULL) {
      return NULL;
    }
    snprintf(default_card, sizeof(default_card), "%scard1.mcd", dir);
    SDL_free(dir);
    p = card_paths[0] = default_card;
  }
  return p == NULL || strcmp(p, "none") == 0 ? NULL : p;
}
int native_frames_run(void) { return frames_run; }

static void write_screenshot(const char *path);

int native_frame_hook(void) {
  frames_run++;
  for (int i = 0; i < n_shots; i++) {
    if (shot_frames[i] == frames_run) {
      char path[1024];
      const char *dot = strrchr(screenshot_path, '.');
      int stem =
          dot ? (int)(dot - screenshot_path) : (int)strlen(screenshot_path);

      snprintf(path, sizeof(path), "%.*s-%d.bmp", stem, screenshot_path,
               frames_run);
      write_screenshot(path);
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
  return frame_limit != 0 && frames_run >= frame_limit;
}

/* Pad bits in PsyZ's order (sdl3_common.h keyb_p1): bit i of the pressed
   mask; the report holds them active-low. */
static const char *const button_names[16] = {
    "l2",     "r2", "l1", "r1",    "triangle", "circle", "cross", "square",
    "select", "l3", "r3", "start", "up",       "right",  "down",  "left"};

static unsigned parse_buttons(const char *s) {
  unsigned mask = 0;

  while (*s != '\0' && *s != ':') {
    size_t n = strcspn(s, "+:");
    int i;

    for (i = 0; i < 16; i++) {
      if (strlen(button_names[i]) == n && strncmp(s, button_names[i], n) == 0) {
        break;
      }
    }
    if (i == 16) {
      fprintf(stderr, "native: unknown button '%.*s'\n", (int)n, s);
      exit(2);
    }
    mask |= 1u << i;
    s += n;
    if (*s == '+') {
      s++;
    }
  }
  return mask;
}

static unsigned scripted_buttons(void) {
  unsigned mask = 0;

  for (int i = 0; i < n_presses; i++) {
    if (frames_run >= presses[i].frame &&
        frames_run < presses[i].frame + presses[i].hold) {
      mask |= presses[i].buttons;
    }
  }
  return mask;
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

static void write_screenshot(const char *path) {
  enum { W = 1024, H = 512 };
  static unsigned short vram[W * H];
  VramRect r = {0, 0, W, H};
  FILE *f;

  Draw_StoreImage(&r, (unsigned long *)vram);
  Draw_ExequeSync();
  f = fopen(path, "wb");
  if (f == NULL) {
    perror(path);
    return;
  }
  fwrite("BM", 1, 2, f);
  put32(f, 54 + W * H * 3);
  put32(f, 0);
  put32(f, 54);
  put32(f, 40);
  put32(f, W);
  put32(f, (unsigned)-H); /* top-down rows */
  put32(f, 1 | (24 << 16));
  put32(f, 0);
  put32(f, W * H * 3);
  put32(f, 2835);
  put32(f, 2835);
  put32(f, 0);
  put32(f, 0);
  for (int i = 0; i < W * H; i++) {
    unsigned p = vram[i];
    unsigned char bgr[3] = {((p >> 10) & 31) << 3, ((p >> 5) & 31) << 3,
                            (p & 31) << 3};

    fwrite(bgr, 1, 3, f);
  }
  fclose(f);
  printf("native: wrote %s\n", path);
}

_Noreturn void native_quit(int code) {
  if (screenshot_path != NULL) {
    write_screenshot(screenshot_path);
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
  unsigned scripted = port == 0 ? scripted_buttons() : 0;

  if (len > 256) {
    len = 256;
  }
  memcpy(dst, src, (size_t)len);
  if (scripted != 0 && len >= 4) {
    if (dst[0] == 0xFF) { /* no host device on the port: plug a digital pad */
      memset(dst, 0, (size_t)len);
      dst[1] = 0x41;
      dst[2] = dst[3] = 0xFF;
    }
    dst[2] &= (unsigned char)~(scripted >> 8);
    dst[3] &= (unsigned char)~scripted;
  }
  w2c_spyro_native_pads_set(&instance, (u32)port, (u32)len);
}

int Psyz_PadsSetKind(int port, int channel, int kind) {
  return (int)w2c_spyro_native_pads_set_kind(&instance, (u32)port,
                                             (u32)channel, (u32)kind);
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
                  "[--screenshot out.bmp] [--shot N]... "
                  "[--press N:BUTTONS[:HOLD]]... [--warp N:LEVEL]... "
                  "[--fast] [--wav out.wav] [--memcard card.mcd] "
                  "[--memcard2 card.mcd] [--log-level debug|info|warn|error] "
                  "[--set KEY=VALUE]... "
                  "[[--config file.toml]... | --no-config]\n"
                  "(--disc is needed unless a config file sets disc)\n");
  exit(2);
}

/* debug|info|warn|error (or its first letter) -> log_level; -1 if neither. */
static int parse_log_level(const char *s) {
  static const char *const names[] = {"debug", "info", "warn", "error"};

  for (int i = 0; i < 4; i++) {
    if (strcmp(s, names[i]) == 0 || (s[0] == names[i][0] && s[1] == '\0')) {
      return i;
    }
  }
  return -1;
}

/* --- settings: the config file and --set --- */

/* Every persistent setting, by its dotted config key. A config file sets
   them by key ([memcard] port1 = ... is memcard.port1), --set KEY=VALUE from
   the command line, and the short flags (--fast, --memcard, ...) are aliases
   for --set. */
enum setting_kind { SET_PATH, SET_BOOL, SET_LOG_LEVEL };
static const struct setting {
  const char *key;
  enum setting_kind kind;
  void *target;
} settings[] = {
    {"disc", SET_PATH, &disc_path},
    {"memcard.port1", SET_PATH, &card_paths[0]},
    {"memcard.port2", SET_PATH, &card_paths[1]},
    {"debug.fast", SET_BOOL, &run_fast},
    {"debug.log_level", SET_LOG_LEVEL, &log_level},
};

/* Keys the first config format had at the top level: reported with the new
   key instead of as unknown. */
static const struct {
  const char *old, *key;
} renamed_keys[] = {
    {"fast", "debug.fast"},
    {"log_level", "debug.log_level"},
    {"memcard", "memcard.port1"},
    {"memcard2", "memcard.port2"},
};

enum { MAX_CONFIGS = 8 };
/* Kept for the whole run: the settings point at their strings. */
static toml_result_t configs[MAX_CONFIGS];
static int n_configs;
static const char *config_file; /* the file being read */
static const char *disc_file;   /* the file that set disc, if one did */

static const struct setting *find_setting(const char *key) {
  for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
    if (strcmp(settings[i].key, key) == 0) {
      return &settings[i];
    }
  }
  return NULL;
}

static const char *setting_want(const struct setting *s) {
  switch (s->kind) {
  case SET_PATH:
    return "a string";
  case SET_BOOL:
    return "true or false";
  case SET_LOG_LEVEL:
    return "\"debug\", \"info\", \"warn\" or \"error\"";
  }
  return "";
}

/* Stores VALUE (a string) in S; 0 if it is not a valid value. */
static int set_setting(const struct setting *s, const char *value) {
  switch (s->kind) {
  case SET_PATH:
    *(const char **)s->target = value;
    return 1;
  case SET_BOOL:
    if (strcmp(value, "true") != 0 && strcmp(value, "false") != 0) {
      return 0;
    }
    *(int *)s->target = value[0] == 't';
    return 1;
  case SET_LOG_LEVEL: {
    int level = parse_log_level(value);

    if (level < 0) {
      return 0;
    }
    *(int *)s->target = level;
    return 1;
  }
  }
  return 0;
}

/* --set KEY=VALUE, and the short flags through it: a bad key or value ends
   the run. */
static void set_from_flag(const char *flag, const char *key,
                          const char *value) {
  const struct setting *s = find_setting(key);

  if (s == NULL) {
    fprintf(stderr, "native: %s: unknown setting %s\n", flag, key);
    exit(2);
  }
  if (!set_setting(s, value)) {
    fprintf(stderr, "native: %s: %s must be %s\n", flag, key,
            setting_want(s));
    exit(2);
  }
  if (s->target == &disc_path) {
    disc_file = NULL;
  }
}

/* A relative path is relative to the config file's directory. */
static const char *config_path(const char *s) {
  const char *slash = strrchr(config_file, '/');
  int dir = slash ? (int)(slash - config_file) + 1 : 0;
  char *p;

  if (s[0] == '/' || dir == 0 || strcmp(s, "none") == 0) {
    return s;
  }
  p = malloc(dir + strlen(s) + 1);
  if (p == NULL) {
    return s;
  }
  memcpy(p, config_file, dir);
  strcpy(p + dir, s);
  return p;
}

static void config_value(const char *key, toml_datum_t v) {
  const struct setting *s = find_setting(key);
  const char *value = NULL;

  if (s == NULL) {
    for (size_t i = 0; i < sizeof(renamed_keys) / sizeof(renamed_keys[0]);
         i++) {
      if (strcmp(renamed_keys[i].old, key) == 0) {
        fprintf(stderr, "native: %s:%d: %s is now %s (ignored)\n",
                config_file, v.lineno, key, renamed_keys[i].key);
        return;
      }
    }
    fprintf(stderr, "native: %s:%d: unknown key %s (ignored)\n", config_file,
            v.lineno, key);
    return;
  }
  if (s->kind == SET_BOOL && v.type == TOML_BOOLEAN) {
    value = v.u.boolean ? "true" : "false";
  } else if (s->kind != SET_BOOL && v.type == TOML_STRING) {
    value = s->kind == SET_PATH ? config_path(v.u.s) : v.u.s;
  }
  if (value == NULL || !set_setting(s, value)) {
    fprintf(stderr, "native: %s:%d: %s must be %s\n", config_file, v.lineno,
            key, setting_want(s));
    exit(2);
  }
  if (s->target == &disc_path) {
    disc_file = config_file;
  }
}

/* Applies every value in TABLE, PREFIX naming the table ("" at the top). */
static void config_table(const char *prefix, toml_datum_t table) {
  for (int i = 0; i < table.u.tab.size; i++) {
    char key[256];
    toml_datum_t v = table.u.tab.value[i];

    snprintf(key, sizeof(key), "%s%s%s", prefix, prefix[0] ? "." : "",
             table.u.tab.key[i]);
    if (v.type == TOML_TABLE) {
      config_table(key, v);
    } else {
      config_value(key, v);
    }
  }
}

/* Reads PATH, or the default config.toml in the user's data directory when
   PATH is NULL (where a missing file is no config). */
static void load_config(const char *path) {
  static char default_config[1024];
  toml_result_t *config;
  FILE *f;

  if (path == NULL) {
    char *dir = SDL_GetPrefPath("open-spyro", "spyro");

    if (dir == NULL) {
      return;
    }
    snprintf(default_config, sizeof(default_config), "%sconfig.toml", dir);
    SDL_free(dir);
    path = default_config;
    if ((f = fopen(path, "rb")) == NULL) {
      return;
    }
  } else if ((f = fopen(path, "rb")) == NULL) {
    fprintf(stderr, "native: cannot read config %s\n", path);
    exit(2);
  }
  config_file = path;
  config = &configs[n_configs++];
  *config = toml_parse_file_named(f, path);
  fclose(f);
  if (!config->ok) {
    fprintf(stderr, "native: %s: %s\n", path, config->errmsg);
    exit(2);
  }
  config_table("", config->toptab);
}

/* The flags other than --config that take the next argument as a value. */
static int takes_value(const char *arg) {
  static const char *const flags[] = {
      "--frames", "--set",      "--disc",     "--screenshot",
      "--shot",   "--press",    "--warp",     "--memcard",
      "--memcard2", "--wav",    "--log-level",
  };

  for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
    if (strcmp(arg, flags[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  u32 table_size;
  int ret;

  setvbuf(stdout, NULL, _IOLBF, 0);
  /* The config files first, in order, so the flags below override them. */
  {
    const char *paths[MAX_CONFIGS];
    int n = 0, none = 0;

    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
        if (n == MAX_CONFIGS) {
          usage();
        }
        paths[n++] = argv[++i];
      } else if (strcmp(argv[i], "--no-config") == 0) {
        none = 1;
      } else if (takes_value(argv[i]) && i + 1 < argc) {
        i++; /* a value, even one that reads --config */
      }
    }
    if (none && n > 0) {
      usage();
    }
    if (n == 0 && !none) {
      load_config(NULL);
    }
    for (int i = 0; i < n; i++) {
      load_config(paths[i]);
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
    } else if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc &&
               n_shots < MAX_SCRIPT) {
      shot_frames[n_shots++] = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--press") == 0 && i + 1 < argc &&
               n_presses < MAX_SCRIPT) {
      const char *a = argv[++i], *b = strchr(a, ':'), *c;

      if (b == NULL) {
        usage();
      }
      c = strchr(b + 1, ':');
      presses[n_presses].frame = atoi(a);
      presses[n_presses].buttons = parse_buttons(b + 1);
      presses[n_presses].hold = c ? atoi(c + 1) : 4;
      n_presses++;
    } else if (strcmp(argv[i], "--warp") == 0 && i + 1 < argc &&
               n_warps < MAX_SCRIPT) {
      const char *a = argv[++i], *b = strchr(a, ':');

      if (b == NULL || atoi(a) <= 0) {
        usage();
      }
      warps[n_warps].frame = atoi(a);
      warps[n_warps].level = atoi(b + 1);
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

  if ((!run_stub && disc_path == NULL) ||
      (n_shots > 0 && screenshot_path == NULL)) {
    usage();
  }
  if (!run_stub) {
    FILE *f = fopen(disc_path, "rb");

    if (f == NULL) {
      if (disc_file != NULL) {
        fprintf(stderr, "native: %s: cannot read disc image %s\n",
                disc_file, disc_path);
      } else {
        fprintf(stderr, "native: cannot read disc image %s\n", disc_path);
      }
      return 2;
    }
    fclose(f);
  }
  wasm_rt_init();
  if (psx_memory_init(&env.memory) != 0) {
    return 1;
  }
  table_size = NATIVE_TABLE_BASE +
               wasm2c_spyro_min_env_0x5F_indirect_function_table;
  if (table_size < NATIVE_TABLE_SIZE) {
    table_size = NATIVE_TABLE_SIZE;
  }
  wasm_rt_allocate_funcref_table(&env.table, table_size, table_size);
  env.memory_base = NATIVE_MODULE_BASE;
  env.table_base = NATIVE_TABLE_BASE;
  env.stack_pointer = NATIVE_STACK_TOP;

  /* GOT.mem imports resolve to PS1 addresses (spyro_w_got.c) and need no
     context. */
  if (run_fast) {
    /* Applied when the backend configures VSync at init. */
    Psyz_VideoSetVsyncMode(PSYZ_VSYNC_LIMITLESS);
  }
  wasm2c_spyro_instantiate(&instance, NULL, &env, &host);
  w2c_spyro_0x5F_wasm_apply_data_relocs(&instance);
  w2c_spyro_0x5F_wasm_call_ctors(&instance);

  if (run_stub) {
    ret = (int)w2c_spyro_native_stub_main(&instance, 0);
  } else {
    u32 arg = w2c_spyro_native_arg_buffer(&instance);

    if (strlen(disc_path) >= 1024) {
      fprintf(stderr, "native: disc path too long\n");
      return 2;
    }
    strcpy(psx_ptr(&env.memory, arg), disc_path);
    ret = (int)w2c_spyro_native_main(&instance);
  }
  native_quit(ret);
}
