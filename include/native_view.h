/* include/native_view.h — the visible screen's horizontal bounds, for culls
 * and full-screen draws that the native build widens (video.aspect).
 *
 * The view is centred on the 512-wide framebuffer at the same projection
 * (SetGeomOffset 0x100, SetGeomScreen 0x155), so only its x bounds change:
 * x VIEW_X0..VIEW_X1 in framebuffer coordinates, -88..600 at 16:9. On the
 * PS1 they are the framebuffer's own 0..SCREEN_W. */

#ifndef OPEN_SPYRO_NATIVE_VIEW_H
#define OPEN_SPYRO_NATIVE_VIEW_H

#include "gpu_prim.h"

#ifdef __psyz
/* Set by the host before the frame that uses them (render/view.c). */
extern int g_nNativeViewX0, g_nNativeViewX1, g_nNativeViewSphereQ12;
#define VIEW_X0 g_nNativeViewX0
#define VIEW_X1 g_nNativeViewX1
#else
#define VIEW_X0 0
#define VIEW_X1 SCREEN_W
#endif

#define VIEW_W (VIEW_X1 - VIEW_X0)
/* Half the view's width from the projection centre (SCREEN_W / 2). */
#define VIEW_HALF_W (SCREEN_W / 2 - VIEW_X0)

/* V scaled from the 4:3 view's half-width to this one's: a cull's
   threshold or its depth term, so the original comparison (4X >= 3Z, say,
   as 4X >= VIEW_WIDEN(3Z)) tests against the wider edge. Exactly V at 4:3,
   so the native build culls as the original there. */
#define VIEW_WIDEN(v) ((int)((long long)(v) * VIEW_HALF_W / (SCREEN_W / 2)))

/* Whether a GTE screen XY word (x in its low half) is left or right of the
   view: the original's `sxy & 0xFE00` outcode test (x outside 0..511 for
   the GTE's -1024..1023) at 4:3. */
#define VIEW_X_OUTSIDE(sxy) ((short)(sxy) < VIEW_X0 || (short)(sxy) >= VIEW_X1)

/* The view's x edges for a GTE screen x held in a word's high half (sxy
   << 16), the outcode tests' form: x > 0 as (x << 16) > VIEW_X0_HI, and x
   < SCREEN_W as (x << 16) - (SCREEN_W << 16) < VIEW_X1_HI_REL. Both 0 at
   4:3. */
#define VIEW_X0_HI (VIEW_X0 * 0x10000)
#define VIEW_X1_HI_REL ((VIEW_X1 - SCREEN_W) * 0x10000)

/* A sphere cull's radius term against the side planes 4|x| = 3kz (k =
   VIEW_HALF_W / 256), from the 4:3 one: it scales with the plane normal's
   length, sqrt(16 + 9k^2) / 5. Exactly V at 4:3. */
#ifdef __psyz
#define VIEW_SPHERE(v) ((int)((long long)(v) * g_nNativeViewSphereQ12 >> 12))
#else
#define VIEW_SPHERE(v) (v)
#endif

#endif
