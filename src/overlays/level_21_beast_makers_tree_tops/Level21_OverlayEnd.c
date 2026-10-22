/* Level21_OverlayEnd (0x80089848, level_21_beast_makers_tree_tops overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_21_beast_makers_tree_tops.ovl.
 */

void Level21_OverlayEnd(void) {}
