/* 0x800133e0 — relocate a freshly loaded actor-mesh table in place (620 b,
   leaf). Every pointer in the table is stored on disc as an offset from the
   table's own base, so loading it means adding the base back in:

     - a table whose count word is negative is a stub: fix up its three
       header pointers and hand back the masked base;
     - otherwise the eight slot pointers at +0x14 are relocated (skipping
       empty ones), the mesh-block delta at +0x34 is rebased, and every live
       per-actor mesh at +0x38 is walked. Each mesh's own pointer fields are
       shifted by the delta, then its packed 21-bit vertex words at +0x24 are
       re-based: flagged meshes halve the rebased offset back into the low
       21 bits, plain ones add the delta to the first word of each pair and
       patch the second word's 16-bit link when it is set. */

int RelocateActorMeshHeader(int *table) {
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
    return (int)table & 0x7FFFFFFF;
  }

  for (i = 0; i < 8; i++) {
    if (table[5 + i] != 0) {
      table[5 + i] = ((int)table & 0x7FFFFFFF) + table[5 + i];
    }
  }

  hdr[13] = (int)table + hdr[13];

  for (i = 0; i < hdr[0]; i++) {
    if (hdr[14 + i] != -1) {
      hdr[14 + i] = (int)table + hdr[14 + i];
      ((int *)hdr[14 + i])[5] = hdr[13] + ((int *)hdr[14 + i])[5];
      ((int *)hdr[14 + i])[6] = hdr[13] + ((int *)hdr[14 + i])[6];
      if (((int *)hdr[14 + i])[7] != 0) {
        ((int *)hdr[14 + i])[7] = hdr[13] + ((int *)hdr[14 + i])[7];
        ((int *)hdr[14 + i])[8] = hdr[13] + ((int *)hdr[14 + i])[8];
      }
      if (*(unsigned char *)(hdr[14 + i] + 4) != 0) {
        ((int *)hdr[14 + i])[4] = hdr[13] + ((int *)hdr[14 + i])[4];
        w = (int *)(hdr[14 + i] + 0x24);
        for (j = 0; j < *(short *)hdr[14 + i]; j++) {
          word = *w;
          lo = word & 0x1FFFFF;
          *w = word & 0xFFE00000;
          lo = ((lo + hdr[13]) >> 1) & 0x1FFFFF;
          *w += lo;
          w++;
        }
      } else {
        w = (int *)(hdr[14 + i] + 0x24);
        for (j = 0; j < *(short *)hdr[14 + i]; j++, w++) {
          *w += hdr[13] & 0x1FFFFF;
          w++;
          if ((*w & 0xFFFF) != 0) {
            *w += ((hdr[13] - (int)w + 4) >> 2) & 0xFFFF;
          }
        }
      }
    }
  }
  return (int)table;
}
