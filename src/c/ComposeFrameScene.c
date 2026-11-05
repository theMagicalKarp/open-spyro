#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* Mid-frame scene composer (0x80019698, 164 bytes): sequences the actor /
   sprite / effect passes of one frame — actor draw lists, a FillWord clear of
   the OT depth-bin heads, sprite prims, actor drop shadows; Spyro's own passes
   (RasterizeSpyroModel + drop shadow) unless g_nSpyroDrawSuppressed; flame
   trails only while g_bSpyroFlameActive; then the world
   particle emitters. */

void ComposeFrameScene(void) {
  BuildActorDrawList();
  FillWord(&g_apMainDrawEntityList, 0, 0x900);
  EmitActorDrawList();
  BuildSecondaryActorDrawList();
  EmitSecondaryActorPrimitives();
  RasterizeSpritePrimQueue();
  DrawActorDropShadows();
  if (g_nSpyroDrawSuppressed == 0) {
    RasterizeSpyroModel();
    DrawSpyroDropShadow();
  }
  if (g_bSpyroFlameActive != 0) {
    DrawSpyroFlameTrails();
  }
  DrawWorldParticleEmitters();
}
