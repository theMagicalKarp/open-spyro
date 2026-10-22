/* Level15_OverlayEnd (0x8008A8A0, level_15_magic_crafters_wizard_peak overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_15_magic_crafters_wizard_peak.ovl.
 */

void Level15_OverlayEnd(void) {}
