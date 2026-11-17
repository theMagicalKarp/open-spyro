/* src/native/wasm/game_prelude.h — force-included (-include) into every game
 * C file of the native build, never the PS1 build.
 *
 * Renames a few game-side names that collide with, or must be routed around,
 * PsyZ's SDK definitions in the one native module. Each rename applies to the
 * game's definition and all its callers alike. */

#ifndef OPEN_SPYRO_GAME_PRELUDE_H
#define OPEN_SPYRO_GAME_PRELUDE_H

/* The game's own `SpuInit` (boot/init.c) resets the memory-card library; it
 * is not libspu's SpuInit, which the game reaches as InitSpu. */
#define SpuInit Game_SpuInit

/* CdRead goes through the SDK shim (sdk_shim.c), which completes the read
 * and raises the CdReadCallback the game registered. */
#define CdRead Shim_CdRead
#define CdReadCallback Shim_CdReadCallback
/* CdControl goes through the shim too, which records the last command. */
#define CdControl Shim_CdControl

/* PsyZ declares these with other return types than the game's prototypes
 * but does not define them; the shim defines them under the game's
 * signature. */
#define LoadExec Shim_LoadExec

/* The game's ordering tables use the PS1 packet format, not PsyZ's native
 * one; sdk_shim.c converts them. */
#define DrawOTag Shim_DrawOTag
#define SetDrawMode Shim_SetDrawMode
#define MulMatrix0 Shim_MulMatrix0

#endif
