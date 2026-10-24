/* fog_color.h — RGB byte views of the world fog colour and of the two frame
 * DRAWENVs' background colour (r0/g0/b0). The overlay draw handlers copy the
 * fog colour into both backgrounds so the clear matches the depth fog.
 *
 * Main-EXE code reads g_dwWorldFogColor as the packed word globals.h declares;
 * only files that do not include globals.h use these views. */

#ifndef OPEN_SPYRO_FOG_COLOR_H
#define OPEN_SPYRO_FOG_COLOR_H

typedef struct Rgb8 {
  unsigned char r, g, b;
} Rgb8;

extern Rgb8 g_dwWorldFogColor;   /* 0x80078a50 */
extern Rgb8 g_bFrameDrawEnv0BgR; /* 0x80076ef9: frame 0 DRAWENV r0/g0/b0 */
extern Rgb8 g_bFrameDrawEnv1BgR; /* 0x80076f7d: frame 1 DRAWENV r0/g0/b0 */

#endif /* OPEN_SPYRO_FOG_COLOR_H */
