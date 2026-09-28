extern int g_pWorkAreaTop[];    /* 0x800785fc */
extern int *g_apActorMeshTable; /* 0x80076378 */

/* The loader rewrites the mesh table under us, so every read of the table
   pointer is a fresh one and every pointer written back into it is published
   immediately. */
#define MESH_TABLE (*(int *volatile *)&g_apActorMeshTable)
#define PUBLISH(p) (*(volatile int *)(p))

/* Load-bearing forms:
 *   - all table/entry accesses are INDEX form off the base, never a pointer
 *     walker, so loop.c keeps +0x38/+0x14 in the addressing mode.
 *   - MESH_TABLE reads are volatile (cse would fold the per-record reloads
 *     into one) and the stores are volatile too (PUBLISH): sched.c orders two
 *     memory refs only when BOTH are volatile, so plain stores let sched1
 *     hoist the next statement's `lui/lw` into the load-delay slots.
 *   - `count` is two statements, or fold reassociates `stream + 0x24` into a
 *     preheader invariant.
 *   - `stream` is pinned to $6: as a pseudo the param's a0 preference keeps
 *     it in a0 (find_reg pass 0 excludes a preferred reg from every other
 *     allocno), and the loop counter can never win a0.
 *   - the relocation sum is pinned to $3 and the inner loop's mesh re-fetch
 *     to $2; `word` is read before the shift so its load fills the lh delay
 *     slot.
 */

/* 0x80013230 — bind a freshly loaded mob-actor mesh stream to the actor mesh
   table (432 b, leaf). First every one of the 46 table slots whose mesh
   pointer still points below the work-area top (or is the -1 empty marker) is
   cleared, then the stream is walked: each record starts with its actor index
   and byte size, and the record body that follows is installed as that
   actor's mesh. The mesh's vertex block sits past its packed-word array, so
   that address becomes the relocation base for the +0x10 field and for the
   packed 21-bit words at +0x24. */
void RelocateMobActorTable(int *arg) {
  register int *stream asm("$6") = arg;
  int i;
  int *mesh;
  int *w;
  int idx;
  int size;
  int base;
  int count;
  int word;
  int lo;

  for (i = 0; i < 0x2E; i++) {
    int *tbl = MESH_TABLE;
    if ((unsigned int)tbl[14 + i] < (unsigned int)g_pWorkAreaTop[0] ||
        tbl[14 + i] == -1) {
      tbl[14 + i] = 0;
    }
  }

  idx = stream[0];
  while (idx >= 0) {
    stream++;
    size = stream[0];
    stream++;

    {
      int *tbl = MESH_TABLE;
      PUBLISH(&tbl[14 + idx]) = (int)stream;
    }
    {
      int *tbl = MESH_TABLE;
      mesh = (int *)tbl[14 + idx];
      PUBLISH(&mesh[5]) = tbl[13] + mesh[5];
    }
    {
      int *tbl = MESH_TABLE;
      mesh = (int *)tbl[14 + idx];
      PUBLISH(&mesh[6]) = tbl[13] + mesh[6];
    }
    mesh = (int *)MESH_TABLE[14 + idx];
    count = *(short *)mesh;
    word = mesh[4];
    count = (count << 2) + 0x24;
    base = (int)stream + count;
    {
      register int sum asm("$3") = base + word;

      PUBLISH(&mesh[4]) = sum;
    }

    {
      register int *m asm("$2") = (int *)MESH_TABLE[14 + idx];
      w = m + 9;
      for (i = 0; i < *(short *)m; i++) {
        word = *w;
        lo = (((word & 0x1FFFFF) + base) >> 1) & 0x1FFFFF;
        *w = (word & 0xFFE00000) + lo;
        w++;
        m = (int *)MESH_TABLE[14 + idx];
      }
    }

    stream = (int *)((int)stream + size);
    idx = stream[0];
  }
}
