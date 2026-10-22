/* Level14_OverlayEnd (0x8008C73C, level_14_magic_crafters_high_caves overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_14_magic_crafters_high_caves.ovl.
 */

void Level14_OverlayEnd(void) {}
