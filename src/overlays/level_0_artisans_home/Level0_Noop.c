/* Level0_Noop (0x80088620, level_0_artisans_home overlay).
 *
 * Empty function (jr ra; nop) in the overlay's .text.
 *
 * Verified byte-identical inside the relinked level_0_artisans_home.ovl.
 */

void Level0_Noop(void) {}
