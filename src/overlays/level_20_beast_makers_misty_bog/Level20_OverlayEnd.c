/* Level20_OverlayEnd (0x80087130, level_20_beast_makers_misty_bog overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_20_beast_makers_misty_bog.ovl.
 */

void Level20_OverlayEnd(void) {}
