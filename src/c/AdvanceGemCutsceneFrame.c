/* globals declared locally: the camera euler trio is written as one short[3]
   (array store, so the loop guard load stays below it — globals.h types the
   three as scalars). */
extern void *g_pPathTableHead; /* 0x80075680 */
extern void *g_pActorListBase; /* 0x80075828 */
extern int g_anCameraPos[];    /* 0x80076df8 */
extern short
    g_nCameraEulerRoll[]; /* 0x80076e1c: [0] roll, [1] pitch, [2] yaw */
extern char *D_8007637C[];

/* 0x8002bfe0 — per-tick gem-cutscene pose update (1088 b, leaf). Reads the
   current path node (timer>>1, stride 0x18) into the camera pos/euler
   globals, then for each cutscene actor derives a segment index + 1/64
   fraction from the actor's control type byte (0x10: /8, 0x15: /6, 0x20: /4,
   else /2), writes the seg/60 keyframe split into the actor record and lerps
   the packed 10-bit path points into the actor position. */
void AdvanceGemCutsceneFrame(int timer) {
  int *path;
  int *hdr;
  int i;
  int seg;
  int frac;
  int inv;
  int *ap;
  int a[3];
  int b[3];
  int pad[2];
  unsigned char type;

  path = (int *)g_pPathTableHead;
  g_anCameraPos[0] = ((int *)path[4])[(path[0] >> 1) * 6];
  g_anCameraPos[1] = ((int *)path[4])[(path[0] >> 1) * 6 + 1];
  g_anCameraPos[2] = ((int *)path[4])[(path[0] >> 1) * 6 + 2];
  g_nCameraEulerRoll[0] = ((int *)path[4])[(path[0] >> 1) * 6 + 3];
  g_nCameraEulerRoll[1] = ((int *)path[4])[(path[0] >> 1) * 6 + 4];
  g_nCameraEulerRoll[2] = ((int *)path[4])[(path[0] >> 1) * 6 + 5];

  i = 0;
  if (path[3] > 0) {
    do {
      type = *(unsigned char *)(*(char **)(D_8007637C[i] + 0x38) + 0xC);
      if (type == 0x10) {
        int t = *(int *)g_pPathTableHead;
        seg = t >> 3;
        frac = (t & 7) << 3;
      } else if (type == 0x15) {
        int t = *(int *)g_pPathTableHead;
        seg = t / 6;
        frac = (t % 6) * 10;
      } else if (type == 0x20) {
        int t = *(int *)g_pPathTableHead;
        seg = t >> 2;
        frac = (t & 3) << 4;
      } else {
        int t = *(int *)g_pPathTableHead;
        seg = t >> 1;
        frac = (t & 1) << 5;
      }
      ((char *)g_pActorListBase)[i * 0x58 + 0x3C] = seg / 60;
      ((char *)g_pActorListBase)[i * 0x58 + 0x3E] = seg % 60;
      ((char *)g_pActorListBase)[i * 0x58 + 0x3D] = (seg + 1) / 60;
      ((char *)g_pActorListBase)[i * 0x58 + 0x3F] = (seg + 1) % 60;
      ((char *)g_pActorListBase)[i * 0x58 + 0x40] = frac;
      hdr = (int *)g_pPathTableHead;
      a[0] = (((int **)hdr)[i + 5][seg] & 0x3FF00000) >> 12;
      a[1] = (((int **)hdr)[i + 5][seg] & 0xFFC00) >> 2;
      a[2] = (((int **)hdr)[i + 5][seg] & 0x3FF) << 8;
      b[0] = (((int **)hdr)[i + 5][seg + 1] & 0x3FF00000) >> 12;
      b[1] = (((int **)hdr)[i + 5][seg + 1] & 0xFFC00) >> 2;
      b[2] = (((int **)hdr)[i + 5][seg + 1] & 0x3FF) << 8;
      inv = 0x40 - frac;
      ap = (int *)g_pActorListBase;
      ap[i * 0x16 + 3] = (a[0] * inv + b[0] * frac) >> 6;
      ap[i * 0x16 + 4] = (a[1] * inv + b[1] * frac) >> 6;
      ap[i * 0x16 + 5] = (a[2] * inv + b[2] * frac) >> 6;
      i++;
    } while (i < hdr[3]);
  }
}
