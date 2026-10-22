/* Level3_OverlayEnd (0x80089ECC, level_3_artisans_town_square overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_3_artisans_town_square.ovl.
 */

void Level3_OverlayEnd(void) {}
