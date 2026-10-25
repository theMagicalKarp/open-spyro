/* pad_buttons.h — controller button bits of g_dwPadHeld / g_dwPadPressed /
 * g_dwPadReleased (the PSY-Q pad word layout: shoulders and face buttons in
 * the low byte, Select/Start and the D-pad in the high byte). Menus move on
 * PAD_UP / PAD_DOWN, confirm with PAD_CROSS; Start pauses, Select opens the
 * inventory. */

#ifndef OPEN_SPYRO_PAD_BUTTONS_H
#define OPEN_SPYRO_PAD_BUTTONS_H

#define PAD_L2 0x0001
#define PAD_R2 0x0002
#define PAD_L1 0x0004
#define PAD_R1 0x0008
#define PAD_TRIANGLE 0x0010
#define PAD_CIRCLE 0x0020
#define PAD_CROSS 0x0040
#define PAD_SQUARE 0x0080
#define PAD_SELECT 0x0100
#define PAD_L3 0x0200
#define PAD_R3 0x0400
#define PAD_START 0x0800
#define PAD_UP 0x1000
#define PAD_RIGHT 0x2000
#define PAD_DOWN 0x4000
#define PAD_LEFT 0x8000

#define PAD_DPAD (PAD_UP | PAD_RIGHT | PAD_DOWN | PAD_LEFT)

#endif /* OPEN_SPYRO_PAD_BUTTONS_H */
