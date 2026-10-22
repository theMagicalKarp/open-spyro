/* Level31_OverlayEnd (0x80088668, level_31_gnastys_world_gnorc_cove overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_31_gnastys_world_gnorc_cove.ovl.
 */

void Level31_OverlayEnd(void) {}
