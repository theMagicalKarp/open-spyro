/* Level6_OverlayEnd (0x8008CFA4, level_6_peace_keepers_home overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_6_peace_keepers_home.ovl.
 */

void Level6_OverlayEnd(void) {}
