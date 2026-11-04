#include "model.h"

/* 0x800133e0 — relocate a freshly loaded actor Model in place (620 b, leaf).
   Every pointer in it is stored on disc as an offset from the model's own
   base, so loading it means adding the base back in:

     - a model whose count word is negative is a stub: fix up its three
       header pointers and hand back the masked base;
     - otherwise the eight m_CollisionModels slots are relocated (skipping
       empty ones), m_Data is rebased, and every present m_Animations header
       is walked: its face / colour pointers move by m_Data, then its frame
       records are rebased. Spyro-format animations (m_IsSpyroAnimation) keep
       one word per frame and halve the rebased offset back into its low 21
       bits; actor animations keep two words per frame, add m_Data to the
       first and patch the second word's 16-bit link when it is set. */

/* Model words and one animation header's words, as int indexes. */
#define DATA hdr[MODEL_WORD(m_Data)]
#define ANIM_SLOT(i) hdr[MODEL_WORD(m_Animations) + (i)]
#define ANIM(i) ((int *)ANIM_SLOT(i))
#define AW(field) ANIM_HEADER_WORD(field)

void *RelocateActorMeshHeader(int *table) {
  int *hdr;
  int *w;
  int i;
  int j;
  int word;
  int lo;

  hdr = table;
  if (table[0] < 0) {
    table[1] = table[1] + (int)table;
    table[2] = table[2] + (int)table;
    table[3] = table[3] + (int)table;
    return (void *)((int)table & 0x7FFFFFFF);
  }

  for (i = 0; i < 8; i++) {
    if (table[MODEL_WORD(m_CollisionModels) + i] != 0) {
      table[MODEL_WORD(m_CollisionModels) + i] =
          ((int)table & 0x7FFFFFFF) + table[MODEL_WORD(m_CollisionModels) + i];
    }
  }

  DATA = (int)table + DATA;

  for (i = 0; i < hdr[0]; i++) {
    if (ANIM_SLOT(i) != -1) {
      ANIM_SLOT(i) = (int)table + ANIM_SLOT(i);
      ANIM(i)[AW(m_Faces)] = DATA + ANIM(i)[AW(m_Faces)];
      ANIM(i)[AW(m_Colors)] = DATA + ANIM(i)[AW(m_Colors)];
      if (ANIM(i)[AW(m_LpFaces)] != 0) {
        ANIM(i)[AW(m_LpFaces)] = DATA + ANIM(i)[AW(m_LpFaces)];
        ANIM(i)[AW(m_LpColors)] = DATA + ANIM(i)[AW(m_LpColors)];
      }
      if (ANIM_HEADER_RAW(ANIM_SLOT(i), unsigned char, m_IsSpyroAnimation) !=
          0) {
        ANIM(i)
        [AW(m_AnimationVertices)] = DATA + ANIM(i)[AW(m_AnimationVertices)];
        w = (int *)(ANIM_SLOT(i) + ANIM_HEADER_OFFSET(m_FrameWords));
        for (j = 0; j < *(short *)ANIM_SLOT(i); j++) {
          word = *w;
          lo = word & 0x1FFFFF;
          *w = word & 0xFFE00000;
          lo = ((lo + DATA) >> 1) & 0x1FFFFF;
          *w += lo;
          w++;
        }
      } else {
        w = (int *)(ANIM_SLOT(i) + ANIM_HEADER_OFFSET(m_FrameWords));
        for (j = 0; j < *(short *)ANIM_SLOT(i); j++, w++) {
          *w += DATA & 0x1FFFFF;
          w++;
          if ((*w & 0xFFFF) != 0) {
            *w += ((DATA - (int)w + 4) >> 2) & 0xFFFF;
          }
        }
      }
    }
  }
  return table;
}
