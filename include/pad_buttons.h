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

/* g_nHitRumbleTimer top-up for a hit or hard landing: vblanks of strong
 * DualShock rumble (PollPadAndDistributeInput counts it down). */
#define HIT_RUMBLE_FRAMES 0xF

/* Pulse-rumble top-ups: g_nPulseRumbleAmount strength and g_nPulseRumbleTimer
 * vblanks (raised to at least these). */
#define LAND_RUMBLE_AMOUNT 0xA0       /* landing out of a FALL (set outright) */
#define LAND_RUMBLE_FRAMES 15
#define BONK_RUMBLE_AMOUNT 0x78       /* a charge bonk */
#define BONK_RUMBLE_FRAMES 0xF
#define CHARGE_HIT_RUMBLE_AMOUNT 0x64 /* a charge hitting an actor */
#define CHARGE_HIT_RUMBLE_FRAMES 0xC
#define SUPERCHARGE_RUMBLE_BASE 0x14  /* supercharge: BASE + (speed - charge floor) >> 7, */
#define SUPERCHARGE_RUMBLE_MIN 0x50   /* clamped to MIN..MAX */
#define SUPERCHARGE_RUMBLE_MAX 0xA0
#define SUPERCHARGE_RUMBLE_FRAMES 4
#define RESCUE_RUMBLE_AMOUNT 0x46     /* a dragon rescue's burst (set outright) */
#define RESCUE_RUMBLE_FRAMES 8
/* g_nVibrationLevel floors. */
#define HURT_TINT_VIBRATION 0x2D      /* SPYRO_ST_HURT_TINT */
#define VIBRATION_TEST_LEVEL 10       /* turning vibration on in the options */

#endif /* OPEN_SPYRO_PAD_BUTTONS_H */
