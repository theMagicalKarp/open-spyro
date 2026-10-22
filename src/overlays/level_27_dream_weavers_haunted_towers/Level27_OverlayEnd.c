/* Level27_OverlayEnd (0x80089820, level_27_dream_weavers_haunted_towers overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_27_dream_weavers_haunted_towers.ovl.
 */

void Level27_OverlayEnd(void) {}
