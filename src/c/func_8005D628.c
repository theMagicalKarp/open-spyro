/* func_8005D628 - VMA 0x8005d628, 144 bytes.

   Scale `a` by (0x103B/0x1000)^(b >> 5) - about 2^(1/48) per whole step,
   the fixed-point factor re-derived each step - then linearly interpolate
   the fractional step (b & 0x1F)/32 between the last two products. Returns
   the result >> 12.

   Matching notes: `lo` is a one-element stack array at sp+8 behind an
   unused 8-byte local (the original keeps it in memory in a 16-byte frame;
   a volatile store would not reach the loop branch's delay slot), the loop is
   guarded by `n != 0` (beqz, with `i = 0` left in place), and `f` is updated in
   two statements so the subtract lands in f's register. */
unsigned int func_8005D628(int a, unsigned int b) {
  int pad[2]; /* unused; puts lo at sp+8 */
  int lo[1];
  unsigned int hi;
  unsigned int f;
  int i;
  int n;

  f = 0x103B;
  lo[0] = a * f;
  hi = a << 12;
  i = 0;
  n = b >> 5;
  b &= 0x1F;
  if (n != 0) {
    do {
      hi = a * f;
      f *= 0x103B;
      f >>= 12;
      lo[0] = a * f;
    } while (++i < n);
  }
  return (hi + ((lo[0] - hi) >> 5) * b) >> 12;
}
