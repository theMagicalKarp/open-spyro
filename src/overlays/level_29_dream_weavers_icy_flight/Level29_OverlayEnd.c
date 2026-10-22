/* Level29_OverlayEnd (0x80084934, level_29_dream_weavers_icy_flight overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_29_dream_weavers_icy_flight.ovl.
 */

void Level29_OverlayEnd(void) {}
