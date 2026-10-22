/* Level16_OverlayEnd (0x8008749C, level_16_magic_crafters_blowhard overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_16_magic_crafters_blowhard.ovl.
 */

void Level16_OverlayEnd(void) {}
