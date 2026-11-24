/* include/native_view.h — the visible screen's horizontal bounds, for culls
 * and full-screen draws that the native build widens (video.aspect).
 *
 * The view is centred on the 512-wide framebuffer at the same projection
 * (SetGeomOffset 0x100, SetGeomScreen 0x155), so only its x bounds change:
 * x VIEW_X0..VIEW_X1 in framebuffer coordinates, -88..600 at 16:9. On the
 * PS1 they are the framebuffer's own 0..SCREEN_W.
 *
 * The culls read CULL_X0..CULL_X1 instead: the view, or wider by
 * debug.cull_margin on each side, so a run with the margin differs from one
 * without only in what the culls drop. The full-screen draws, the HUD and
 * the camera keep the view itself.
 *
 * The view is symmetric about the projection centre x = SCREEN_W / 2:
 * VIEW_X0 + VIEW_X1 == SCREEN_W. The slope-based culls (CULL_HALF_W and the
 * macros built on it) read only CULL_X0, the screen-space tests both edges;
 * the host warns if PsyZ ever hands it an off-centre canvas. */

#ifndef OPEN_SPYRO_NATIVE_VIEW_H
#define OPEN_SPYRO_NATIVE_VIEW_H

#include "gpu_prim.h"

#ifdef __psyz
/* Set by the host before the frame that uses them (render/view.c). */
extern int g_nNativeViewX0, g_nNativeViewX1, g_nNativeViewSphereQ12;
extern int g_nNativeCullX0, g_nNativeCullX1;
#define VIEW_X0 g_nNativeViewX0
#define VIEW_X1 g_nNativeViewX1
#define CULL_X0 g_nNativeCullX0
#define CULL_X1 g_nNativeCullX1
/* The menu states draw a frame captured from VRAM as their backdrop, which
   only holds the 512-wide framebuffer. With a widened view, their capture
   frame squeezes the whole view into it (NativeMenuBackdrop, before the
   state's draw) and the backdrop's quads stretch it back out
   (NativeStretchBackdrop, on the four quads and the line after them).
   render/view.c. */
void NativeMenuBackdrop(void);
void NativeStretchBackdrop(PolyFT4 *quads);
/* video.hud = "edges": the HUD's side counters move out with a widened
   view's edges, the gem counter and egg row to the left one, the lives and
   key icons to the right one; the dragon counter stays centred. The icon
   records and egg rects InitHudCounters lays out at their 4:3 rest
   positions are shifted by the view's edges (NativeHudReset, after that
   layout; NativeHudAnchor, each HUD draw, following a resized view), and
   the gem total's text by NativeHudLeftDx. Flight levels keep their own
   HUD, as laid out. render/view.c. */
void NativeHudReset(void);
void NativeHudAnchor(void);
int NativeHudLeftDx(void);
/* The projection's screen distance (SetGeomScreen): a camera-space x at
   depth z lands z / VIEW_SCREEN_H times as far from the centre on screen. */
#define VIEW_SCREEN_H 0x155
#else
#define VIEW_X0 0
#define VIEW_X1 SCREEN_W
#define CULL_X0 0
#define CULL_X1 SCREEN_W
#endif

#define VIEW_W (VIEW_X1 - VIEW_X0)
#define CULL_W (CULL_X1 - CULL_X0)
/* Half the culls' width from the projection centre (SCREEN_W / 2). */
#define CULL_HALF_W (SCREEN_W / 2 - CULL_X0)

/* The cull macros below all read the cull bounds, not the view's. */

/* V scaled from the 4:3 view's half-width to the culls': a cull's
   threshold or its depth term, so the original comparison (4X >= 3Z, say,
   as 4X >= VIEW_WIDEN(3Z)) tests against the wider edge. Exactly V at 4:3,
   so the native build culls as the original there. */
#define VIEW_WIDEN(v) ((int)((long long)(v) * CULL_HALF_W / (SCREEN_W / 2)))

/* Whether a GTE screen XY word (x in its low half) is left or right of the
   view: the original's `sxy & 0xFE00` outcode test (x outside 0..511 for
   the GTE's -1024..1023) at 4:3. */
#define VIEW_X_OUTSIDE(sxy) ((short)(sxy) < CULL_X0 || (short)(sxy) >= CULL_X1)

/* The view's x edges for a GTE screen x held in a word's high half (sxy
   << 16), the outcode tests' form: x > 0 as (x << 16) > VIEW_X0_HI, and x
   < SCREEN_W as (x << 16) - (SCREEN_W << 16) < VIEW_X1_HI_REL. Both 0 at
   4:3. */
#define VIEW_X0_HI (CULL_X0 * 0x10000)
#define VIEW_X1_HI_REL ((CULL_X1 - SCREEN_W) * 0x10000)

/* A near-plane margin R scaled down as the view widens (R at 4:3): a test
   that keeps out objects within R of the camera plane, because at 4:3 they
   were off screen, keeps out fewer of those the wider view does show. */
#define VIEW_NARROW(r) ((int)((long long)(r) * (SCREEN_W / 2) / CULL_HALF_W))

/* A sphere cull's radius term against the side planes 4|x| = 3kz (k =
   CULL_HALF_W / 256), from the 4:3 one: it scales with the plane normal's
   length, sqrt(16 + 9k^2) / 5. Exactly V at 4:3. */
#ifdef __psyz
#define VIEW_SPHERE(v) ((int)((long long)(v) * g_nNativeViewSphereQ12 >> 12))
#else
#define VIEW_SPHERE(v) (v)
#endif

#endif
