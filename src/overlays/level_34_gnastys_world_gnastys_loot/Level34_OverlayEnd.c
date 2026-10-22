/* Level34_OverlayEnd (0x80086264, level_34_gnastys_world_gnastys_loot overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_34_gnastys_world_gnastys_loot.ovl.
 */

void Level34_OverlayEnd(void) {}
