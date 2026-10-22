/* Level33_OverlayEnd (0x800854B4, level_33_gnastys_world_gnasty_gnorc overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_33_gnastys_world_gnasty_gnorc.ovl.
 */

void Level33_OverlayEnd(void) {}
