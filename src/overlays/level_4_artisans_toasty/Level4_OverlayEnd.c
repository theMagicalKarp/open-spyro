/* Level4_OverlayEnd (0x80084A10, level_4_artisans_toasty overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_4_artisans_toasty.ovl.
 */

void Level4_OverlayEnd(void) {}
