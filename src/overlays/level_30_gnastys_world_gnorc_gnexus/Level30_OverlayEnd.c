/* Level30_OverlayEnd (0x80085CE0, level_30_gnastys_world_gnorc_gnexus overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_30_gnastys_world_gnorc_gnexus.ovl.
 */

void Level30_OverlayEnd(void) {}
