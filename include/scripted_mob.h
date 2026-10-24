/* scripted_mob.h — the scripted "mob" records listed by
 * g_apScriptedMobEntries (g_nScriptedMobCount entries), unpacked from the
 * level bundle by UnpackWorldDataChunks and drawn by DrawActors. */

#ifndef OPEN_SPYRO_SCRIPTED_MOB_H
#define OPEN_SPYRO_SCRIPTED_MOB_H

/* One scripted-mob mesh header; only the tint word at +0x10 is read here. */
typedef struct ScriptedMobMesh {
  int pad00;
  int pad04;
  int pad08;
  int pad0C;
  unsigned int color; /* +0x10 */
} ScriptedMobMesh;

/* A scripted "mob" billboard: a convex ring of world-space points that gets
   projected to screen space and stroked as an outline. */
typedef struct ScriptedMob {
  ScriptedMobMesh *pMesh; /* +0x00 */
  int nPoints;            /* +0x04 */
  int nx;                 /* +0x08 */
  int ny;                 /* +0x0c */
  int nz;                 /* +0x10 */
  int nGateIndex;         /* +0x14 */
  int pad18;
  int pad1C;
  int pts[8][3]; /* +0x20 */
} ScriptedMob;

#endif /* OPEN_SPYRO_SCRIPTED_MOB_H */
