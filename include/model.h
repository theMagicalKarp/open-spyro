/* model.h — an actor model (g_apActorMeshTable entry): sound ids, collision
 * models and one header per animation. Spyro's own model is entry 0. */

#ifndef OPEN_SPYRO_MODEL_H
#define OPEN_SPYRO_MODEL_H

typedef struct AnimationHeader {
  short m_NumFrames;              /* 0x00 */
  unsigned short m_NumColors;     /* 0x02 */
  unsigned char m_IsSpyroAnimation; /* 0x04 */
  unsigned char m_Scale;
  unsigned char m_ShortEncodeShift;
  unsigned char m_Radius;
  unsigned char m_VertCountHigh;  /* 0x08 */
  unsigned char m_VertCountLow;
  unsigned char m_Padding2;
  unsigned char m_DepthScale;
  unsigned char m_ProgressPerTick; /* 0x0C -- the byte TYPE_DESC reads */
  unsigned char m_Padding3;
  unsigned short m_Padding4;
  void *m_AnimationVertices;      /* 0x10 */
  void *m_Faces;
  void *m_Colors;
  void *m_LpFaces;
  void *m_LpColors;               /* 0x20 */
  unsigned int m_FrameWords[1];   /* 0x24 one word per frame; byte 3 is the
                                   * frame's footstep sfx id (0xFF = none,
                                   * TickSpyroGameplayFrame) */
} AnimationHeader;

typedef struct Model {
  int m_NumAnimations;            /* 0x00 */
  unsigned char m_Sounds[16];     /* 0x04 */
  void *m_CollisionModels[8];     /* 0x14 */
  void *m_Data;                   /* 0x34 */
  AnimationHeader *m_Animations[1]; /* 0x38 */
} Model;

/* Byte offsets of Model / AnimationHeader fields, for matched code that walks
 * the tables with int arithmetic. */
#define MODEL_OFFSET(field) ((int)&((Model *)0)->field)
#define ANIM_HEADER_OFFSET(field) ((int)&((AnimationHeader *)0)->field)

#endif /* OPEN_SPYRO_MODEL_H */
