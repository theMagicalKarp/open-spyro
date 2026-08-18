/* func_level_29_8007B4B0 (0x8007B4B0, level_29_dream_weavers_icy_flight overlay, 476 bytes).
 *
 * Format and draw the flight-level race time at `pos` (units of 1/6 second).
 * Negative time draws the placeholder string and returns 6. Otherwise formats
 * h:mm:ss ("%d %02d.%02d") or mm:ss ("%d.%02d") into a stack buffer,
 * right-aligns it by the glyph count (9px per glyph), draws it, and for the
 * hour form additionally draws the colon overlay twice at 1.5x width.
 * Returns the glyph count.
 *
 * Word-identical body in all five flight overlays (5/11/17/23/29).
 *
 * The whole function turns on ONE source decision: `min` must be computed off
 * `q600` BEFORE the copy that feeds the *100 chain. See cookbook A210.
 */

extern void func_80017FE4(); /* draw text run */
extern void func_80062FD4(); /* sprintf */
extern int func_8006276C();  /* strlen */

int func_level_29_8007B4B0(int time, int *pos, int arg2)
{
  char buf[16];
  int q600;
  int hr;
  int min;
  int frac;
  int len;
  int sign;

  if (time < 0)
  {
    pos[0] -= 0x36;
    pos[1] -= 5;
    func_80017FE4("......", pos, 0x12, arg2);
    sign = 6;
    return sign;
  }

  sign = time >> 31;
  q600 = time / 600;
  hr = time / 36000;
  min = q600 - (hr * 60);
  frac = q600;
  frac = (time / 6) - (frac * 100);

  if (hr != 0)
  {
    func_80062FD4(buf, "%d %02d.%02d", hr, min, frac);
  }
  else
  {
    func_80062FD4(buf, "%d.%02d", min, frac);
  }
  len = func_8006276C(buf);
  pos[0] -= len * 9;
  func_80017FE4(buf, pos, 0x12, arg2);
  if (hr != 0)
  {
    char *fmt = ".";
    pos[0] -= 0x6C;
    pos[1] += 1;
    pos[2] = (pos[2] * 3) >> 1;
    func_80017FE4(fmt, pos, 0x12, arg2);
    len += 1;
    pos[0] -= 0x12;
    pos[1] -= 8;
    func_80017FE4(fmt, pos, 0x12, arg2);
  }
  return len;
}
