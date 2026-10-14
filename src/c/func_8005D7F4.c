/* func_8005D7F4 - VMA 0x8005d7f4, 296 bytes.

   Inverse of ComputeSpuPitchFromNoteDelta: find the top clear bit of ~pitch
   (the octave), then walk 48 1/4-semitone steps of 0x103B/0x1000 with 32
   linear sub-steps each until the pitch falls inside one; returns
   ((note + semitones + octave*12) << 8) | (fine + cents).

   Matching notes: the bit-scan and step counters are one variable, pinned
   to t2 (A374: k/n/a must be emitted in that order, which shortens a's live
   range enough to win the global-alloc tie for t2). The hit block sits in
   the OUTER loop after a `continue`, so reorg steals `v = n + k` from the
   branch target into the delay slot and deletes the block; n = i*32 is a
   source local so that steal is one insn. x is shifted inside the test so
   sched2 puts y's srl in the first branch's delay slot. */
unsigned int func_8005D7F4(unsigned short note, unsigned short fine,
                           unsigned short pitch) {
  register int i asm("$10");
  int hb;
  int oct;
  int base;
  unsigned int f;
  int k;
  unsigned int lo;
  unsigned int hi;
  unsigned int step;
  unsigned int a;
  unsigned int b;
  unsigned int x;
  unsigned int y;
  int v;
  int q;
  int r;
  int hiw;
  unsigned short np;
  int n;
  int low;

  np = ~pitch;
  hb = 0;
  for (i = 15; i >= 0; i--) {
    if (((np >> i) & 1) == 0) {
      hb = i;
      break;
    }
  }
  oct = hb - 12;
  base = 1 << hb;
  f = 0x1000;
  for (i = 0; i < 0x30; i++) {
    lo = base * f;
    f *= 0x103B;
    f >>= 12;
    hi = base * f;
    step = (hi - lo) >> 5;
    k = 0;
    n = i * 32;
    a = 0;
    b = step;
    for (; k < 32; k++) {
      x = lo + a;
      y = (lo + b) >> 12;
      if (pitch >= x >> 12) {
        if (pitch < y) {
          goto hit;
        }
      }
      b += step;
      a += step;
    }
    continue;
  hit:
    v = n + k;
    goto found;
  }
  v = 0x600;
found:
  q = v / 128;
  r = v - q * 128;
  hiw = note + q + oct * 12;
  low = fine + r;
  return (hiw << 8) | low;
}
