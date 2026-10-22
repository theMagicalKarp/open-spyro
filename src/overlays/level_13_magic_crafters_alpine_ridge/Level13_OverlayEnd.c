/* Level13_OverlayEnd (0x8008DEC0, level_13_magic_crafters_alpine_ridge overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_13_magic_crafters_alpine_ridge.ovl.
 */

void Level13_OverlayEnd(void) {}
