#include "model.h"
#include "spyro_state.h"

extern int g_pWorkAreaTop[];    /* 0x800785fc */
extern int *g_apActorMeshTable; /* 0x80076378 */

/* The loader rewrites Spyro's model under us, so every read of the table
   pointer is a fresh one and every pointer written back into it is published
   immediately. */
#define SPYRO_MODEL (*(int *volatile *)&g_apActorMeshTable)
#define PUBLISH(p) (*(volatile int *)(p))
/* Model.m_Animations as an int-word index */
#define ANIMS MODEL_WORD(m_Animations)

/* Load-bearing forms:
 *   - all table/entry accesses are INDEX form off the base, never a pointer
 *     walker, so loop.c keeps +0x38/+0x14 in the addressing mode.
 *   - SPYRO_MODEL reads are volatile (cse would fold the per-record reloads
 *     into one) and the stores are volatile too (PUBLISH): sched.c orders two
 *     memory refs only when BOTH are volatile, so plain stores let sched1
 *     hoist the next statement's `lui/lw` into the load-delay slots.
 *   - `count` is two statements, or fold reassociates `stream + 0x24` into a
 *     preheader invariant.
 *   - `stream` is pinned to $6: as a pseudo the param's a0 preference keeps
 *     it in a0 (find_reg pass 0 excludes a preferred reg from every other
 *     allocno), and the loop counter can never win a0.
 *   - the relocation sum is pinned to $3 and the inner loop's header re-fetch
 *     to $2; `word` is read before the shift so its load fills the lh delay
 *     slot.
 */

/* 0x80013230 — install a streamed set of Spyro animations into Spyro's model
   (g_apActorMeshTable[0], read here as the bare `int *` it holds; 432 b,
   leaf). First every one of the model's 46 m_Animations slots whose header
   still points below the work-area top (or is the -1 empty marker) is
   cleared, then the stream is walked: each record starts with its animation
   index and byte size, and the AnimationHeader that follows is installed in
   that slot. Its m_Faces / m_Colors pointers are rebased by the model's
   m_Data; the vertex block sits past the one-word-per-frame records, so that
   address becomes the base for m_AnimationVertices and for the packed 21-bit
   (>> 1) vertex offset in every m_FrameWords word. */
void InstallSpyroAnimStream(int *table) {
  /* Hard-register pins (A374 / F14j): each holds the register the
     original uses for it, which settles the competing temps. */
  register int *stream asm("$6") = table;
  int i;
  int *anim;
  int *w;
  int idx;
  int size;
  int base;
  int count;
  int word;
  int lo;

  for (i = 0; i < SPYRO_ANIMID_COUNT; i++) {
    int *tbl = SPYRO_MODEL;
    if ((unsigned int)tbl[ANIMS + i] < (unsigned int)g_pWorkAreaTop[0] ||
        tbl[ANIMS + i] == -1) {
      tbl[ANIMS + i] = 0;
    }
  }

  idx = stream[0];
  while (idx >= 0) {
    stream++;
    size = stream[0];
    stream++;

    {
      int *tbl = SPYRO_MODEL;
      PUBLISH(&tbl[ANIMS + idx]) = (int)stream;
    }
    {
      int *tbl = SPYRO_MODEL;
      anim = (int *)tbl[ANIMS + idx];
      PUBLISH(&anim[ANIM_HEADER_WORD(m_Faces)]) =
          tbl[MODEL_WORD(m_Data)] + anim[ANIM_HEADER_WORD(m_Faces)];
    }
    {
      int *tbl = SPYRO_MODEL;
      anim = (int *)tbl[ANIMS + idx];
      PUBLISH(&anim[ANIM_HEADER_WORD(m_Colors)]) =
          tbl[MODEL_WORD(m_Data)] + anim[ANIM_HEADER_WORD(m_Colors)];
    }
    anim = (int *)SPYRO_MODEL[ANIMS + idx];
    count = *(short *)anim;
    word = anim[ANIM_HEADER_WORD(m_AnimationVertices)];
    count = (count << 2) + ANIM_HEADER_OFFSET(m_FrameWords);
    base = (int)stream + count;
    {
      register int sum asm("$3") = base + word;

      PUBLISH(&anim[ANIM_HEADER_WORD(m_AnimationVertices)]) = sum;
    }

    {
      register int *m asm("$2") = (int *)SPYRO_MODEL[ANIMS + idx];
      w = m + ANIM_HEADER_WORD(m_FrameWords);
      for (i = 0; i < *(short *)m; i++) {
        word = *w;
        lo = (((word & 0x1FFFFF) + base) >> 1) & 0x1FFFFF;
        *w = (word & 0xFFE00000) + lo;
        w++;
        m = (int *)SPYRO_MODEL[ANIMS + idx];
      }
    }

    stream = (int *)((int)stream + size);
    idx = stream[0];
  }
}
