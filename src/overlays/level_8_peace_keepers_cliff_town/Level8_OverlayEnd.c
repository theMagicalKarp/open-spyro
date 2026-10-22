/* Level8_OverlayEnd (0x8008BAF8, level_8_peace_keepers_cliff_town overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_8_peace_keepers_cliff_town.ovl.
 */

void Level8_OverlayEnd(void) {}
