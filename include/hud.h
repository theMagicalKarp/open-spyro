/* hud.h — byte-offset accessors for the HUD record tables.
 *
 * The HUD code walks its tables with one byte offset per record (slot * 8 for
 * the sprite / rest-pose RECTs, slot * 0x58 for the icon actors, slot * 12 for
 * the icon rest positions) and often shares that offset between two tables.
 * These accessors keep the byte-offset form, which is what produces the
 * original's $at-indexed %hi/%lo accesses, while naming the record; indexing a
 * typed array instead lets loop.c hoist the table bases (cookbook A453). */

#ifndef OPEN_SPYRO_HUD_H
#define OPEN_SPYRO_HUD_H

#include "actor_type.h"
#include "globals.h"

/* Icon actor record at byte offset `o` (slot * 0x58) of g_abHudIconActorRecords. */
#define HUD_ICON(o) ((Actor *)((char *)g_abHudIconActorRecords + (o)))

/* Icon actor record `slot` (a constant slot folds to the same address). */
#define HUD_ICON_SLOT(slot) HUD_ICON((slot) * ACTOR_SIZE)
#define HUD_SLOT_BUTTERFLY 10 /* the sparx-food butterflies are laid out around it */
#define HUD_SLOT_KEY 11       /* the level key icon (ACTOR_TYPE_KEY) */

/* RECT at byte offset `off` (slot * 8) of a HUD rect table
 * (g_anHudSpriteRects, g_anHudDigitRestRects). */
#define HUD_RECT(tbl, off) ((RECT *)((char *)(tbl) + (off)))

/* Icon rest position (x, y, z words) at byte offset `o` (slot * 12) of
 * g_anHudIconRestPos. */
#define HUD_REST_POS(o) ((int *)((char *)g_anHudIconRestPos + (o)))

#endif /* OPEN_SPYRO_HUD_H */
