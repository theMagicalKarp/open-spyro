/* Level11_OverlayEnd (0x80083BF0, level_11_peace_keepers_night_flight overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_11_peace_keepers_night_flight.ovl.
 */

void Level11_OverlayEnd(void) {}
