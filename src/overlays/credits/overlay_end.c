/* Credits_OverlayEnd (0x8007C338, credits overlay).
 *
 * Empty function closing the overlay's .text. Credits_Update uses its address
 * as the start of the free pool after the overlay (the level overlays mark
 * their end the same way, see Level0_OverlayEnd).
 *
 * Verified byte-identical inside the relinked credits.ovl.
 */

void Credits_OverlayEnd(void) {}
