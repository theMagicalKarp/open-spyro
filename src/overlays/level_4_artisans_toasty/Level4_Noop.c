/* Level4_Noop (0x80084A10, level_4_artisans_toasty overlay).
 *
 * Empty function (jr ra; nop) in the overlay's .text.
 *
 * Verified byte-identical inside the relinked level_4_artisans_toasty.ovl.
 */

void Level4_Noop(void) {}
