#include "globals.h"

/* Record the newly pressed buttons in the pad-history ring and scan it
   against the cheat-code sequence table (0x8002d8ec, 0x188). For each of
   the 8 sequences (0x40 bytes apart) counts its length, rewinds the ring
   cursor that many entries, and compares; on a full match applies the
   cheat effect and clears the ring. Only active while the CD stream is
   idle. */
extern void ApplyCheatCodeEffect(int code);
extern void ClearPadHistoryRing(void);
void CheckCheatCodeMatch(void) {
  int pad[2];
  int idx;
  int off;
  int next;
  int *ring;
  int *base;
  int *seq;
  int *p;
  int *q;
  int *h;
  int len;
  int start;
  int i;
  int match;
  int code;
  if (g_nCdStreamState < 0) {
    code = g_dwPadPressed;
    if (code != 0) {
      idx = g_nPadHistoryIndex;
      next = idx + 1;
      off = idx << 2;
      ring = g_anPadHistoryRing;
      *((int *)(((char *)ring) + off)) = code;
      g_nPadHistoryIndex = next;
      if (next >= 0x10) {
        g_nPadHistoryIndex = 0;
      }
      code = 0;
      base = ring;
      seq = g_anCheatCodeSequences;
      do {
        len = 0;
        if (seq[0] != 0) {
          p = seq;
        count_top:
          if (len < 0x10) {
            p += 1;
            len += 1;
            if ((*p) != 0) {
              goto count_top;
            }
          }
        }
        if (len == 0) {
          goto advance;
        }
        {
          start = g_nPadHistoryIndex;
          for (i = 0; i < len; i++) {
            start -= 1;
            if (start < 0) {
              start = 0xF;
            }
          }

          match = 1;
          i = 0;
          if (len > 0) {
            q = seq;
            h = (int *)((start << 2) + ((int)base));
            do {
              if (match == 0) {
                goto advance;
              }
              if ((*h) != (*q)) {
                match = 0;
              }
              h += 1;
              start += 1;
              if (start >= 0x10) {
                h = base;
                start = 0;
              }
              i += 1;
              q += 1;
            } while (i < len);
          }
          if (match != 0) {
            ApplyCheatCodeEffect(code);
            ClearPadHistoryRing();
            return;
          }
        }
      advance:
        code += 1;

        seq += 0x10;
      } while (code < 8);
    }
  }
}
