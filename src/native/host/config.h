/* src/native/host/config.h — the native executable's settings: config files,
 * --set, and the command-line value formats (config.c).
 *
 * Kept free of SDL and the wasm module so it builds into the host unit tests
 * (native/tests). Parsing reports problems on config_log and returns nonzero;
 * the caller decides whether that ends the run. */

#ifndef OPEN_SPYRO_NATIVE_CONFIG_H
#define OPEN_SPYRO_NATIVE_CONFIG_H

#include <stdio.h>

#include "stick.h"

/* Every persistent setting, by its dotted config key (config.c settings). */
typedef struct NativeSettings {
  const char *disc;    /* disc */
  const char *card[2]; /* memcard.port1, memcard.port2 */
  int fast;            /* debug.fast */
  int log_level;       /* debug.log_level: 0 debug .. 3 error */
  int overlay;         /* debug.overlay: start with the diagnostics shown */
  int start_level;     /* debug.start.level: level id to boot into, 0 = off */
  /* debug.cull_margin: game pixels the side culls reach past each edge of a
     widened view, to show what they drop (0 = the view itself). */
  int cull_margin;
  int analog;          /* input.pad: 1 "analog" (a DualShock), 0 "digital" */
  StickShape stick[2]; /* input.analog.left.*, input.analog.right.* */
  /* input.camera.*: mode "analog" (camera_analog = 1) lets the right stick
     turn the camera (not in the original), at `speed` times the L2/R2 rate;
     held at full deflection it speeds up to `boost` times that over
     `boost_time` seconds. */
  int camera_analog;
  int camera_invert; /* input.camera.invert: stick right turns like L2 */
  double camera_speed, camera_boost, camera_boost_time;
  /* video.internal_resolution: the render scale, 1..8, or 0 for the window
     size over the canvas; 1 keeps PsyZ's PS1 display mode. */
  int internal_res;
  /* video.aspect: the view's width over its height, 4:3 (the original) to
     8:3 (PsyZ's widest canvas), wider showing more of the world at the
     sides; 0 for the window's. */
  double aspect;
  /* The config file that set disc, if one did (for its error message). */
  const char *disc_file;
} NativeSettings;

enum { CONFIG_MAX_FILES = 8 };

/* The settings a run starts from, before any config file. */
#define NATIVE_SETTINGS_DEFAULT                                                \
  {                                                                            \
      .log_level = 1 /* info */,                                               \
      .analog = 1,                                                             \
      .stick = {STICK_SHAPE_ORIGINAL, {STICK_MODE_RADIAL, {0.15, 1.0}, 1.0}},  \
      .camera_speed = 1.0,                                                     \
      .camera_boost = 1.0,                                                     \
      .camera_boost_time = 0.5,                                                \
      .internal_res = 1,                                                       \
      .aspect = 4.0 / 3.0,                                                     \
  }

/* Where parse problems and config warnings go; stderr unless changed. */
extern FILE *config_log;

/* debug|info|warn|error (or its first letter) -> 0..3; -1 if neither. */
int config_parse_log_level(const char *s);

/* Pad button names joined by '+' ("cross+up"), up to the end or a ':', as
   a mask in PsyZ's bit order. Nonzero (reported) on an unknown name. */
int config_parse_buttons(const char *s, unsigned *mask);

/* --press N:BUTTONS[:HOLD] (HOLD defaults to 4). 1 if the form is wrong,
   2 (reported) on an unknown button. */
int config_parse_press(const char *arg, int *frame, unsigned *buttons,
                       int *hold);

/* Whether LEVEL is a level id (world * 10 + n: worlds 1..6, n 0..5, but
   Gnasty's world has no 65). */
int config_level_valid(int level);

/* --warp N:LEVEL; nonzero if the form is wrong or N is not positive. */
int config_parse_warp(const char *arg, int *frame, int *level);

/* video.aspect's range: the original 4:3, up to the 1024-wide canvas PsyZ
   can draw (8:3 at 240 lines). */
#define CONFIG_ASPECT_MIN (4.0 / 3.0)
#define CONFIG_ASPECT_MAX (8.0 / 3.0)

/* "auto" (0), "W:H" or a number, as width over height, into *ASPECT;
   nonzero if malformed or outside CONFIG_ASPECT_MIN..MAX. */
int config_parse_aspect(const char *s, double *aspect);

/* The canvas width to ask PsyZ for at 240 lines and ASPECT (window
   pixels: 320 is 4:3, a 512x240 canvas), clamped to the range above. */
int config_aspect_request_width(double aspect);

/* Whether ARG is a flag other than --config that takes the next argument. */
int config_takes_value(const char *arg);

/* The first pass over argv: the --config paths in order (into PATHS, *N of
   them) and whether --no-config was given. Nonzero if more than
   CONFIG_MAX_FILES are named or --config is mixed with --no-config. */
int config_scan_args(int argc, char **argv, const char *paths[CONFIG_MAX_FILES],
                     int *n, int *none);

/* --set KEY=VALUE (and the short flags through it); FLAG names the flag in
   the message. Nonzero (reported) on an unknown key or a bad value. */
int config_set(NativeSettings *s, const char *flag, const char *key,
               const char *value);

/* PATH relative to the directory of CONFIG_FILE, unless it is absolute or
   "none" (returned as is). The result may be a new allocation. */
const char *config_relative_path(const char *config_file, const char *path);

/* Applies the TOML file PATH. A missing file is an error unless OPTIONAL
   (then it is no config). Nonzero (reported) if it cannot be read or parsed,
   or has an unknown key or table or a value of the wrong type or range;
   every such key is reported, not only the first. */
int config_load_file(NativeSettings *s, const char *path, int optional);

/* Applies TOML source TEXT as if read from the file NAME. */
int config_load_string(NativeSettings *s, const char *name, const char *text);

#endif
