/* The visible screen's horizontal bounds (include/native_view.h), pushed by
 * the host whenever video.aspect's canvas changes. Not in the original game:
 * there the screen is always the framebuffer. */

#include "camera.h"
#include "frame_db.h"
#include "gamestate.h"
#include "globals.h"
#include "hud.h"
#include "native_view.h"

int g_nNativeViewX0 = 0, g_nNativeViewX1 = SCREEN_W;
int g_nNativeCullX0 = 0, g_nNativeCullX1 = SCREEN_W;
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

/* X0..X1 in framebuffer coordinates, X0 <= 0 and X1 >= SCREEN_W; the culls
   reach MARGIN past each side (debug.cull_margin). */
__attribute__((export_name("native_set_view"))) void
native_set_view(int x0, int x1, int margin) {
  unsigned long long h;

  g_nNativeViewX0 = x0;
  g_nNativeViewX1 = x1;
  g_nNativeCullX0 = x0 - margin;
  g_nNativeCullX1 = x1 + margin;
  /* sqrt(16 + 9k^2) / 5 in Q12, k = CULL_HALF_W / 256: with both under the
     root scaled by 256^2, sqrt(16 * 256^2 + 9 * CULL_HALF_W^2) / (5 * 256),
     1280 / 1280 at 4:3. Rounded up, so a sphere is never culled early. */
  h = (unsigned long long)CULL_HALF_W;
  g_nNativeViewSphereQ12 =
      (int)((((unsigned long long)ISqrt(16ull * 65536 + 9 * h * h) + 1) * 4096 +
             1279) /
            1280);
  if (CULL_HALF_W == SCREEN_W / 2) {
    g_nNativeViewSphereQ12 = 1 << 12;
  }
}

/* The menu states' backdrop is the frame their first draw renders, read
   back from VRAM's 512-wide framebuffer (PauseMenu_Draw). With a widened
   view, that frame's world-to-screen matrix (BuildCameraViewMatrix, rebuilt
   every frame) scales camera x by SCREEN_W / VIEW_W, so the whole view lands
   in the framebuffer. The culls keep the wide view's bounds, which hold all
   of the squeezed frame. Later frames clear the sides to black, not the
   world's fog colour; the stretched backdrop covers them but for its rows
   above and below. */
void NativeMenuBackdrop(void) {
  short *row;
  int i;

  if (VIEW_X0 == 0) {
    return;
  }
  switch (g_nGamestate) {
  case GS_PAUSE_MENU:
  case GS_INVENTORY:
  case GS_UNUSED_6:
  case GS_FLIGHT_RESULTS:
    if (g_nMenuDrawFrames == 0) {
      /* The capture frame keeps the fog colour, as at 4:3. */
      row = (short *)CAM_W2C_MTX_VIA_EULER;
      for (i = 0; i < 3; i++) {
        row[i] = (short)(row[i] * SCREEN_W / VIEW_W);
      }
      break;
    }
    /* fall through */
  case GS_MENU_TRANSITION:
    /* GamestateDraw sets the fog colour again on the next gameplay frame. */
    g_FrameDrawEnv0.r0 = g_FrameDrawEnv0.g0 = g_FrameDrawEnv0.b0 = 0;
    g_FrameDrawEnv1.r0 = g_FrameDrawEnv1.g0 = g_FrameDrawEnv1.b0 = 0;
    break;
  default:
    break;
  }
}

/* The backdrop's MENU_BACKDROP_STRIPS quads, laid out across the 512-wide
   framebuffer, and the black line under them, spread over the view. */
void NativeStretchBackdrop(PolyFT4 *quads) {
  LineF2 *line = (LineF2 *)(quads + MENU_BACKDROP_STRIPS);
  int i;

  if (VIEW_X0 == 0) {
    return;
  }
  for (i = 0; i < MENU_BACKDROP_STRIPS; i++) {
    quads[i].x0 = quads[i].x2 = VIEW_X0 + VIEW_W * i / MENU_BACKDROP_STRIPS;
    quads[i].x1 = quads[i].x3 =
        VIEW_X0 + VIEW_W * (i + 1) / MENU_BACKDROP_STRIPS;
  }
  line->x0 = VIEW_X0;
  line->x1 = VIEW_X1;
}

/* video.hud: 1 for "edges" (native_set_hud, from the host at start). */
static int hudEdges;
/* The shift the HUD records hold now, relative to their 4:3 rest positions:
   the left group's and the right group's. */
static int hudLeftDx, hudRightDx;

/* InitHudCounters' 12 egg rects, before the butterflies' (0x60 bytes in). */
#define HUD_EGG_RECTS 12

__attribute__((export_name("native_set_hud"))) void native_set_hud(int edges) {
  hudEdges = edges;
}

static void ShiftHud(int left, int right) {
  int i;

  for (i = HUD_SLOT_GEM_DIGITS; i <= HUD_SLOT_GEMS; i++) {
    HUD_ICON_SLOT(i)->posX += left;
  }
  for (i = HUD_SLOT_LIVES_DIGITS; i <= HUD_SLOT_KEY; i++) {
    HUD_ICON_SLOT(i)->posX += right;
  }
  /* The butterflies follow the lives icon: UpdateHudCounters lays them out
     around its posX every frame. */
  for (i = 0; i < HUD_EGG_RECTS; i++) {
    HUD_RECT(g_anHudSpriteRects, i * 8)->x += left;
  }
}

void NativeHudAnchor(void) {
  int left = 0, right = 0;

  if (g_nFlightLevelActive != 0) {
    return;
  }
  if (hudEdges) {
    left = VIEW_X0;
    right = VIEW_X1 - SCREEN_W;
  }
  if (left != hudLeftDx || right != hudRightDx) {
    ShiftHud(left - hudLeftDx, right - hudRightDx);
    hudLeftDx = left;
    hudRightDx = right;
  }
}

void NativeHudReset(void) {
  hudLeftDx = hudRightDx = 0;
  NativeHudAnchor();
}

int NativeHudLeftDx(void) { return hudLeftDx; }
