/* Level26_OverlayEnd (0x8008771C, level_26_dream_weavers_lofty_castle overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_26_dream_weavers_lofty_castle.ovl.
 */

void Level26_OverlayEnd(void) {}
