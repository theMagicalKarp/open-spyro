/* src/native/host/script.h — the scripted run (--press, --shot): which pad
 * buttons are held on a frame, how they reach pad 1's report and where each
 * --shot screenshot goes. Free of wasm2c and SDL so native/tests can test it
 * (script_test.c). */

#ifndef OPEN_SPYRO_SCRIPT_H
#define OPEN_SPYRO_SCRIPT_H

#include <stddef.h>

/* --press N:BUTTONS[:HOLD]: `buttons` (config_parse_buttons bits) are held
   for `hold` VSyncs from VSync `frame`. */
typedef struct {
  int frame, hold;
  unsigned buttons;
} ScriptPress;

/* The buttons the presses hold at VSync `frame`. */
unsigned script_buttons(const ScriptPress *presses, int n, int frame);

/* Holds `buttons` on top of a raw pad report of `len` bytes (active-low
   button bytes 2 and 3). A port with no device (report[0] 0xFF) becomes a
   digital pad first. Reports shorter than 4 bytes, and no buttons, are left
   as they are. */
void script_apply_pad(unsigned char *report, int len, unsigned buttons);

/* The --shot path for VSync `frame`: the --screenshot path with its
   extension replaced by "-<frame>.bmp". */
void script_shot_path(char *buf, size_t size, const char *screenshot,
                      int frame);

#endif
