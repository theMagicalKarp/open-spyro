/* cutscene_path.h — the scripted-cutscene path table (g_pPathTableHead).
 *
 * TickCutsceneBundleLoadStream streams it in with the cutscene bundle and
 * rebases `camera` and the per-actor `track` pointers; Cutscene_Update ticks
 * `frame` and AdvanceCutsceneFrame poses the camera and every cutscene actor
 * from it. The title screen loops its intro through the same record. */

#ifndef OPEN_SPYRO_CUTSCENE_PATH_H
#define OPEN_SPYRO_CUTSCENE_PATH_H

typedef struct CutscenePath {
  int frame;      /* 0x00 cutscene clock: two ticks per path node */
  int pad04;      /* 0x04 */
  int length;     /* 0x08 path nodes; the cutscene ends at frame length * 2 */
  int actorCount; /* 0x0C cutscene actors, from g_pActorListBase[0] */
  int *camera;    /* 0x10 per node: camera x, y, z, roll, pitch, yaw */
  int *track[1];  /* 0x14 per actor: one packed 10:10:10 position per
                   * animation segment */
} CutscenePath;

#endif /* OPEN_SPYRO_CUTSCENE_PATH_H */
