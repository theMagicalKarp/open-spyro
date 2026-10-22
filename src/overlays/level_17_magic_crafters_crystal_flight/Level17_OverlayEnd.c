/* Level17_OverlayEnd (0x80084390, level_17_magic_crafters_crystal_flight overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_17_magic_crafters_crystal_flight.ovl.
 */

void Level17_OverlayEnd(void) {}
