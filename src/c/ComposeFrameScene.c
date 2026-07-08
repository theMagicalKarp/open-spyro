#include "globals.h"

/* Mid-frame scene composer (0x80019698, 164 bytes): sequences the actor /
   sprite / effect passes of one frame — actor draw lists, a FillWord clear of
   the OT depth-bin heads, sprite prims, entity spikes; Spyro's own passes
   (RasterizePairedActor + drop shadow) unless g_nSpyroDrawSuppressed; horn
   strike trails only while the horn collision is active; then the world
   particle emitters. */
extern void BuildActorDrawList(void);
extern void FillWord();
extern void EmitActorDrawList(void);
extern void BuildSecondaryActorDrawList(void);
extern void EmitSecondaryActorPrimitives(void);
extern void RasterizeSpritePrimQueue(void);
extern void DrawEntityDirectionalSpikes(void);
extern void RasterizePairedActor(void);
extern void DrawSpyroDropShadow(void);
extern void DrawSpyroHornStrikeTrails(void);
extern void DrawWorldParticleEmitters(void);

void ComposeFrameScene(void) {
  BuildActorDrawList();
  FillWord(&g_pOtDepthBinHead0, 0, 0x900);
  EmitActorDrawList();
  BuildSecondaryActorDrawList();
  EmitSecondaryActorPrimitives();
  RasterizeSpritePrimQueue();
  DrawEntityDirectionalSpikes();
  if (g_nSpyroDrawSuppressed == 0) {
    RasterizePairedActor();
    DrawSpyroDropShadow();
  }
  if (g_bSpyroHornCollisionActive != 0) {
    DrawSpyroHornStrikeTrails();
  }
  DrawWorldParticleEmitters();
}
