/* ComputeSpuPitchFromNoteDelta - VMA 0x8005d6b8, 316 bytes.

   SPU pitch for playing a sample recorded at (note, fine) as (baseNote,
   baseFine), both in 1/128-semitone units: 0x1000 shifted by the whole
   octaves of the difference (0x600 = 12 semitones), then scaled by
   0x103B/0x1000 (about 2^(1/48)) per 1/4-semitone step with linear
   interpolation of the remaining 1/32 steps (the same loop as
   func_8005D628, written out inline). Clamped to 0x3FFF.

   Matching notes: `d` doubles as the octave base after its last test and
   `rem` as the step count (both share registers in the original); the
   step remainder's abs is a ?: so expand's singleton path copies it before
   the test; `lo` is the same sp+8 stack array as in func_8005D628. */
unsigned short ComputeSpuPitchFromNoteDelta(unsigned short note,
                                            unsigned short fine,
                                            unsigned short baseNote,
                                            unsigned short baseFine) {
  int pad[2]; /* unused; puts lo at sp+8 */
  int lo[1];
  int from;
  int d;
  int ad;
  int oct;
  int rem;
  int t;
  unsigned int hi;
  unsigned int f;
  int i;
  unsigned int b;
  unsigned int m;
  unsigned int r;

  from = (note << 7) + fine;
  d = ((baseNote << 7) + baseFine) - from;
  ad = d;
  if (d < 0) {
    ad = -d;
  }
  oct = ad / 0x600;
  rem = ad % 0x600;
  if (d >= 0) {
    t = 0x1000 << oct;
  } else {
    if (rem != 0) {
      oct++;
      rem = 0x600 - rem;
    }
    t = 0x1000 >> oct;
  }
  d = (unsigned short)t;
  f = 0x103B;
  lo[0] = d * f;
  hi = d << 12;
  i = 0;
  b = rem < 0 ? -rem : rem;
  rem = b >> 5;
  m = b & 0x1F;
  if (rem != 0) {
    do {
      hi = d * f;
      f *= 0x103B;
      f >>= 12;
      lo[0] = d * f;
    } while (++i < rem);
  }
  r = (hi + ((lo[0] - hi) >> 5) * m) >> 12;
  if (r >= 0x4000) {
    r = 0x3FFF;
  }
  return r;
}
