/* src/native/host/config.c — the native executable's settings (config.h).
 *
 * The persistent settings have dotted keys (the settings table below). A
 * config file sets them by key ([memcard] port1 = ... is memcard.port1),
 * --set KEY=VALUE from the command line, and the short flags (--fast,
 * --memcard, ...) are aliases for --set. Relative paths in a file are
 * relative to it. */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "tomlc17.h"

FILE *config_log;

#define LOG (config_log ? config_log : stderr)

int config_parse_log_level(const char *s) {
  static const char *const names[] = {"debug", "info", "warn", "error"};

  for (int i = 0; i < 4; i++) {
    if (strcmp(s, names[i]) == 0 || (s[0] == names[i][0] && s[1] == '\0')) {
      return i;
    }
  }
  return -1;
}

/* Pad bits in PsyZ's order (sdl3_common.h keyb_p1): bit i of the pressed
   mask; the report holds them active-low. */
static const char *const button_names[16] = {
    "l2",     "r2", "l1", "r1",    "triangle", "circle", "cross", "square",
    "select", "l3", "r3", "start", "up",       "right",  "down",  "left"};

int config_parse_buttons(const char *s, unsigned *mask) {
  *mask = 0;
  while (*s != '\0' && *s != ':') {
    size_t n = strcspn(s, "+:");
    int i;

    for (i = 0; i < 16; i++) {
      if (strlen(button_names[i]) == n && strncmp(s, button_names[i], n) == 0) {
        break;
      }
    }
    if (i == 16) {
      fprintf(LOG, "native: unknown button '%.*s'\n", (int)n, s);
      return 1;
    }
    *mask |= 1u << i;
    s += n;
    if (*s == '+') {
      s++;
    }
  }
  return 0;
}

int config_parse_press(const char *arg, int *frame, unsigned *buttons,
                       int *hold) {
  const char *b = strchr(arg, ':'), *c;

  if (b == NULL) {
    return 1;
  }
  c = strchr(b + 1, ':');
  if (config_parse_buttons(b + 1, buttons) != 0) {
    return 2;
  }
  *frame = atoi(arg);
  *hold = c ? atoi(c + 1) : 4;
  return 0;
}

int config_parse_warp(const char *arg, int *frame, int *level) {
  const char *b = strchr(arg, ':');

  if (b == NULL || atoi(arg) <= 0) {
    return 1;
  }
  *frame = atoi(arg);
  *level = atoi(b + 1);
  return 0;
}

int config_takes_value(const char *arg) {
  static const char *const flags[] = {
      "--frames",   "--set",   "--disc",      "--screenshot",
      "--shot",     "--press", "--warp",      "--memcard",
      "--memcard2", "--wav",   "--log-level",
  };

  for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
    if (strcmp(arg, flags[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

int config_scan_args(int argc, char **argv, const char *paths[CONFIG_MAX_FILES],
                     int *n, int *none) {
  *n = 0;
  *none = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      if (*n == CONFIG_MAX_FILES) {
        return 1;
      }
      paths[(*n)++] = argv[++i];
    } else if (strcmp(argv[i], "--no-config") == 0) {
      *none = 1;
    } else if (config_takes_value(argv[i]) && i + 1 < argc) {
      i++; /* a value, even one that reads --config */
    }
  }
  return *none && *n > 0;
}

/* --- the settings table --- */

enum setting_kind { SET_PATH, SET_BOOL, SET_LOG_LEVEL };
static const struct setting {
  const char *key;
  enum setting_kind kind;
  size_t offset; /* into NativeSettings */
} settings[] = {
    {"disc", SET_PATH, offsetof(NativeSettings, disc)},
    {"memcard.port1", SET_PATH, offsetof(NativeSettings, card[0])},
    {"memcard.port2", SET_PATH, offsetof(NativeSettings, card[1])},
    {"debug.fast", SET_BOOL, offsetof(NativeSettings, fast)},
    {"debug.log_level", SET_LOG_LEVEL, offsetof(NativeSettings, log_level)},
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

static const struct setting *find_setting(const char *key) {
  for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
    if (strcmp(settings[i].key, key) == 0) {
      return &settings[i];
    }
  }
  return NULL;
}

static const char *setting_want(const struct setting *t) {
  switch (t->kind) {
  case SET_PATH:
    return "a string";
  case SET_BOOL:
    return "true or false";
  case SET_LOG_LEVEL:
    return "\"debug\", \"info\", \"warn\" or \"error\"";
  }
  return "";
}

/* Stores VALUE (a string, kept by reference) in T's field of S; 0 if it is
   not a valid value. */
static int store_setting(NativeSettings *s, const struct setting *t,
                         const char *value) {
  void *field = (char *)s + t->offset;

  switch (t->kind) {
  case SET_PATH:
    *(const char **)field = value;
    return 1;
  case SET_BOOL:
    if (strcmp(value, "true") != 0 && strcmp(value, "false") != 0) {
      return 0;
    }
    *(int *)field = value[0] == 't';
    return 1;
  case SET_LOG_LEVEL: {
    int level = config_parse_log_level(value);

    if (level < 0) {
      return 0;
    }
    *(int *)field = level;
    return 1;
  }
  }
  return 0;
}

int config_set(NativeSettings *s, const char *flag, const char *key,
               const char *value) {
  const struct setting *t = find_setting(key);

  if (t == NULL) {
    fprintf(LOG, "native: %s: unknown setting %s\n", flag, key);
    return 1;
  }
  if (!store_setting(s, t, value)) {
    fprintf(LOG, "native: %s: %s must be %s\n", flag, key, setting_want(t));
    return 1;
  }
  if (t->offset == offsetof(NativeSettings, disc)) {
    s->disc_file = NULL;
  }
  return 0;
}

/* --- config files --- */

const char *config_relative_path(const char *config_file, const char *path) {
  const char *slash = strrchr(config_file, '/');
  size_t dir = slash ? (size_t)(slash - config_file) + 1 : 0;
  char *p;

  if (path[0] == '/' || dir == 0 || strcmp(path, "none") == 0) {
    return path;
  }
  p = malloc(dir + strlen(path) + 1);
  if (p == NULL) {
    return path;
  }
  memcpy(p, config_file, dir);
  strcpy(p + dir, path);
  return p;
}

/* A copy that outlives the parse result (the settings point at it). */
static const char *keep(const char *s) {
  char *p = malloc(strlen(s) + 1);

  return p ? strcpy(p, s) : s;
}

static int config_value(NativeSettings *s, const char *file, const char *key,
                        toml_datum_t v) {
  const struct setting *t = find_setting(key);
  const char *value = NULL;

  if (t == NULL) {
    for (size_t i = 0; i < sizeof(renamed_keys) / sizeof(renamed_keys[0]);
         i++) {
      if (strcmp(renamed_keys[i].old, key) == 0) {
        fprintf(LOG, "native: %s:%d: %s is now %s (ignored)\n", file, v.lineno,
                key, renamed_keys[i].key);
        return 0;
      }
    }
    fprintf(LOG, "native: %s:%d: unknown key %s (ignored)\n", file, v.lineno,
            key);
    return 0;
  }
  if (t->kind == SET_BOOL && v.type == TOML_BOOLEAN) {
    value = v.u.boolean ? "true" : "false";
  } else if (t->kind != SET_BOOL && v.type == TOML_STRING) {
    value = t->kind == SET_PATH ? config_relative_path(file, v.u.s) : v.u.s;
    if (value == v.u.s) {
      value = keep(value);
    }
  }
  if (value == NULL || !store_setting(s, t, value)) {
    fprintf(LOG, "native: %s:%d: %s must be %s\n", file, v.lineno, key,
            setting_want(t));
    return 1;
  }
  if (t->offset == offsetof(NativeSettings, disc)) {
    s->disc_file = file;
  }
  return 0;
}

/* Applies every value in TABLE, PREFIX naming the table ("" at the top). */
static int config_table(NativeSettings *s, const char *file, const char *prefix,
                        toml_datum_t table) {
  for (int i = 0; i < table.u.tab.size; i++) {
    char key[256];
    toml_datum_t v = table.u.tab.value[i];
    int err;

    snprintf(key, sizeof(key), "%s%s%s", prefix, prefix[0] ? "." : "",
             table.u.tab.key[i]);
    err = v.type == TOML_TABLE ? config_table(s, file, key, v)
                               : config_value(s, file, key, v);
    if (err) {
      return err;
    }
  }
  return 0;
}

static int apply(NativeSettings *s, const char *name, toml_result_t r) {
  int err;

  if (!r.ok) {
    fprintf(LOG, "native: %s: %s\n", name, r.errmsg);
    toml_free(r);
    return 1;
  }
  err = config_table(s, name, "", r.toptab);
  toml_free(r);
  return err;
}

int config_load_file(NativeSettings *s, const char *path, int optional) {
  FILE *f = fopen(path, "rb");
  toml_result_t r;

  if (f == NULL) {
    if (optional) {
      return 0;
    }
    fprintf(LOG, "native: cannot read config %s\n", path);
    return 1;
  }
  r = toml_parse_file_named(f, path);
  fclose(f);
  return apply(s, keep(path), r);
}

int config_load_string(NativeSettings *s, const char *name, const char *text) {
  return apply(s, keep(name), toml_parse_named(text, (int)strlen(text), name));
}
