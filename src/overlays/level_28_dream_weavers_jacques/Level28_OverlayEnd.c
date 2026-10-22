/* Level28_OverlayEnd (0x80086348, level_28_dream_weavers_jacques overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_28_dream_weavers_jacques.ovl.
 */

void Level28_OverlayEnd(void) {}
