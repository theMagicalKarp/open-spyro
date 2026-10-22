/* Level32_OverlayEnd (0x80086004, level_32_gnastys_world_twilight_harbor overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_32_gnastys_world_twilight_harbor.ovl.
 */

void Level32_OverlayEnd(void) {}
