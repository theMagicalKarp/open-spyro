/* Level9_OverlayEnd (0x80086260, level_9_peace_keepers_ice_cavern overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_9_peace_keepers_ice_cavern.ovl.
 */

void Level9_OverlayEnd(void) {}
