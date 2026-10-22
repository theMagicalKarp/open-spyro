/* Level7_OverlayEnd (0x8008D600, level_7_peace_keepers_dry_canyon overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_7_peace_keepers_dry_canyon.ovl.
 */

void Level7_OverlayEnd(void) {}
