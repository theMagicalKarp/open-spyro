/* src/native/host/script.c — see script.h. */

#include "script.h"

#include <stdio.h>
#include <string.h>

unsigned script_buttons(const ScriptPress *presses, int n, int frame) {
  unsigned mask = 0;

  for (int i = 0; i < n; i++) {
    if (frame >= presses[i].frame &&
        frame < presses[i].frame + presses[i].hold) {
      mask |= presses[i].buttons;
    }
  }
  return mask;
}

void script_apply_pad(unsigned char *report, int len, unsigned buttons) {
  if (buttons == 0 || len < 4) {
    return;
  }
  if (report[0] == 0xFF) { /* no host device on the port: plug a digital pad */
    memset(report, 0, (size_t)len);
    report[1] = 0x41;
    report[2] = report[3] = 0xFF;
  }
  report[2] &= (unsigned char)~(buttons >> 8);
  report[3] &= (unsigned char)~buttons;
}

void script_shot_path(char *buf, size_t size, const char *screenshot,
                      int frame) {
  const char *dot = strrchr(screenshot, '.');
  int stem = dot ? (int)(dot - screenshot) : (int)strlen(screenshot);

  snprintf(buf, size, "%.*s-%d.bmp", stem, screenshot, frame);
}
