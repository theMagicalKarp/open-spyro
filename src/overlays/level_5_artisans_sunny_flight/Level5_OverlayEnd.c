/* Level5_OverlayEnd (0x80084ED0, level_5_artisans_sunny_flight overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_5_artisans_sunny_flight.ovl.
 */

void Level5_OverlayEnd(void) {}
