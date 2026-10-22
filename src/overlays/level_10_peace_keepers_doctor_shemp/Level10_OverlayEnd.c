/* Level10_OverlayEnd (0x80087210, level_10_peace_keepers_doctor_shemp overlay).
 *
 * Empty function (jr ra; nop): the last 8 bytes of the overlay image. Its
 * address marks where the overlay ends, so ApplyPerLevelGlobalsTable installs
 * it as g_pDrawBufA, the start of free memory for level data loaded after it.
 *
 * Verified byte-identical inside the relinked level_10_peace_keepers_doctor_shemp.ovl.
 */

void Level10_OverlayEnd(void) {}
