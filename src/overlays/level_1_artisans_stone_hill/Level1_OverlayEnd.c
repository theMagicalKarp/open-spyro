/* Level1_OverlayEnd (0x8008A3B8, level_1_artisans_stone_hill overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_1_artisans_stone_hill.ovl.
 */

void Level1_OverlayEnd(void) {}
