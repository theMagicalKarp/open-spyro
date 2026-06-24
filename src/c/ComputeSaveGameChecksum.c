/* ComputeSaveGameChecksum (0x8005956c) — toolchain spike.
 *
 * First matched function. Sums the 1420 (0x58C) bytes of a save-game block and
 * returns the total. A reloc-free leaf in the pure-`addu` game-code region, so
 * the rebuilt object's .text compares byte-for-byte without a link step.
 *
 * Verified byte-identical to the original at its VMA.
 */

int ComputeSaveGameChecksum(unsigned char *data) {
  unsigned char *end;
  int sum;

  sum = 0;
  end = data + 1420;
  do {
    sum += *data;
    data++;
  } while (data < end);
  return sum;
}
