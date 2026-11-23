/* The visible screen's horizontal bounds (include/native_view.h), pushed by
 * the host whenever video.aspect's canvas changes. Not in the original game:
 * there the screen is always the framebuffer. */

#include "native_view.h"

int g_nNativeViewX0 = 0, g_nNativeViewX1 = SCREEN_W;
int g_nNativeViewSphereQ12 = 1 << 12;

static unsigned ISqrt(unsigned long long v) {
  unsigned long long r = 0, bit = 1ull << 62;

  while (bit > v) {
    bit >>= 2;
  }
  while (bit != 0) {
    if (v >= r + bit) {
      v -= r + bit;
      r = (r >> 1) + bit;
    } else {
      r >>= 1;
    }
    bit >>= 2;
  }
  return (unsigned)r;
}

/* X0..X1 in framebuffer coordinates, X0 <= 0 and X1 >= SCREEN_W. */
__attribute__((export_name("native_set_view"))) void native_set_view(int x0,
                                                                     int x1) {
  unsigned long long h;

  g_nNativeViewX0 = x0;
  g_nNativeViewX1 = x1;
  /* sqrt(16 + 9k^2) / 5 in Q12, k = VIEW_HALF_W / 256: with both under the
     root scaled by 256^2, sqrt(16 * 256^2 + 9 * VIEW_HALF_W^2) / (5 * 256),
     1280 / 1280 at 4:3. Rounded up, so a sphere is never culled early. */
  h = (unsigned long long)VIEW_HALF_W;
  g_nNativeViewSphereQ12 =
      (int)((((unsigned long long)ISqrt(16ull * 65536 + 9 * h * h) + 1) * 4096 +
             1279) /
            1280);
  if (VIEW_HALF_W == SCREEN_W / 2) {
    g_nNativeViewSphereQ12 = 1 << 12;
  }
}
