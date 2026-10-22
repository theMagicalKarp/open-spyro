/* Level25_OverlayEnd (0x800880D4, level_25_dream_weavers_dark_passage overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_25_dream_weavers_dark_passage.ovl.
 */

void Level25_OverlayEnd(void) {}
