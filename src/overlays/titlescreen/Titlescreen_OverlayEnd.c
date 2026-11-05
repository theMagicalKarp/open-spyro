/* Titlescreen_OverlayEnd (0x8007DDE8, titlescreen overlay).
 *
 * Empty function closing the overlay's .text. Main addresses it as
 * g_abTitlescreenOverlayEnd: the first free byte past the titlescreen, where
 * the title / demo / menu paths point g_pLoadStagingBuffer.
 *
 * Verified byte-identical inside the relinked titlescreen.ovl.
 */

void Titlescreen_OverlayEnd(void) {}
