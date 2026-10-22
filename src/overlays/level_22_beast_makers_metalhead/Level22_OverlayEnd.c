/* Level22_OverlayEnd (0x8008A69C, level_22_beast_makers_metalhead overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_22_beast_makers_metalhead.ovl.
 */

void Level22_OverlayEnd(void) {}
