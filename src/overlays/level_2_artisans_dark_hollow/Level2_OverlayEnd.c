/* Level2_OverlayEnd (0x80085594, level_2_artisans_dark_hollow overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_2_artisans_dark_hollow.ovl.
 */

void Level2_OverlayEnd(void) {}
