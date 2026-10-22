/* Level19_OverlayEnd (0x80087944, level_19_beast_makers_terrace_village overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_19_beast_makers_terrace_village.ovl.
 */

void Level19_OverlayEnd(void) {}
